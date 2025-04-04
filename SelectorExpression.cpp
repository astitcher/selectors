/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "SelectorExpression.h"

#include "selectors.h"

#include "SelectorEnv.h"
#include "SelectorToken.h"
#include "SelectorValue.h"

#include <cstdint>
#include <cstdlib>
#include <cerrno> // Need to use errno in checking return from strtoull()/strtod()
#include <memory>
#include <ostream>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <sstream>
#include <vector>

using std::enable_if_t;
using std::make_unique;
using std::ostream;
using std::string;
using std::string_view;
using std::ostringstream;
using std::unique_ptr;
using std::vector;


/*
 * Syntax for JMS style selector expressions (informal):
 * This is a mixture of regular expression and EBNF formalism
 *
 * The top level term is SelectExpression
 *
 * // Lexical elements
 *
 * Alpha ::= [a-zA-Z]
 * Digit ::= [0-9]
 * HexDigit ::= [0-9a-fA-F]
 * OctDigit ::= [0-7]
 * BinDigit ::= [0-1]
 *
 * IdentifierInitial ::= Alpha | "_" | "$"
 * IdentifierPart ::= IdentifierInitial | Digit | "."
 * Identifier ::= IdentifierInitial IdentifierPart*
 * Constraint : Identifier NOT IN ("NULL", "TRUE", "FALSE", "NOT", "AND", "OR", "BETWEEN", "LIKE", "IN", "IS") // Case insensitive
 *
 * LiteralString ::= ("'" [^']* "'")+ // Repeats to cope with embedded single quote
 *
 * // LiteralExactNumeric is a little simplified as it also allows underscores ("_") as internal seperators and suffix "l" or "L"
 * LiteralExactNumeric ::= "0x" HexDigit+ | "0X" HexDigit+ | "0b" BinDigit+ | "0B" BinDigit+ | "0" OctDigit* | Digit+
 *
 * // LiteralApproxNumeric is a little simplified as it also allows suffix "d", "D", "f", "F"
 * Exponent ::= ('+'|'-')? LiteralExactNumeric
 * LiteralApproxNumeric ::= ( Digit "." Digit* ( "E" Exponent )? ) |
 *                          ( "." Digit+ ( "E" Exponent )? ) |
 *                          ( Digit+ "E" Exponent )
 * LiteralBool ::= "TRUE" | "FALSE"
 *
 * Literal ::= LiteralBool | LiteralString | LiteralApproxNumeric | LiteralExactNumeric
 *
 * EqOps ::= "=" | "<>"
 * ComparisonOps ::= EqOps | ">" | ">=" | "<" | "<="
 * AddOps ::= "+" | "-"
 * MultiplyOps ::= "*" | "/"
 *
 * // Expression Syntax
 *
 * SelectExpression ::= OrExpression? // An empty expression is equivalent to "true"
 *
 * OrExpression ::= AndExpression  ( "OR" AndExpression )*
 *
 * AndExpression :: = ComparisonExpression ( "AND" ComparisonExpression )*
 *
 * ComparisonExpression ::= AddExpression "IS" "NOT"? "NULL" |
 *                          AddExpression "NOT"? "LIKE" LiteralString [ "ESCAPE" LiteralString ] |
 *                          AddExpression "NOT"? "BETWEEN" AddExpression "AND" AddExpression |
 *                          AddExpression "NOT"? "IN" "(" PrimaryExpression ("," PrimaryExpression)* ")" |
 *                          AddExpression ComparisonOps AddExpression |
 *                          "NOT" ComparisonExpression |
 *                          AddExpression
 *
 * AddExpression :: = MultiplyExpression (  AddOps MultiplyExpression )*
 *
 * MultiplyExpression :: = UnaryArithExpression ( MultiplyOps UnaryArithExpression )*
 *
 * UnaryArithExpression ::= "-" LiteralExactNumeric |  // This is a special case to simplify negative ints
 *                          AddOps AddExpression |
 *                          "(" OrExpression ")" |
 *                          PrimaryExpression
 *
 * PrimaryExpression :: = Identifier |
 *                        Literal
 */

namespace selector {

// Define operator<< for classes that have repr()
// need to define type trait has_repr for those clases
template<class T>
struct has_repr{static constexpr bool value = false;};

template<class T>
inline constexpr bool has_repr_v = has_repr<T>::value;

template <class T>
auto inline operator<<(ostream& o, const T& t) -> enable_if_t<has_repr_v<T>, ostream&>
{
    t.repr(o);
    return o;
}

////////////////////////////////////////////////////

// Operators

using CompFn = auto (Value, Value) -> bool;

class ComparisonOperator {
    const char* repr_;
    CompFn& fn_;

public:
    constexpr ComparisonOperator(const char* r, CompFn* fn) :
        repr_(r),
        fn_(*fn)
    {}

    auto repr(ostream& o) const -> void {
        o << repr_;
    }

    auto eval(Expression& e1, Expression& e2, const Env& env) const -> BoolOrNone {
        const Value v1(e1.eval(env));
        if (!unknown(v1)) {
          const Value v2(e2.eval(env));
          if (!unknown(v2)) {
            return BoolOrNone(fn_(v1, v2));
          }
        }
        return BN_UNKNOWN;
    }
};

template <> struct has_repr<ComparisonOperator> {static constexpr bool value = true;};

using UBoolFn = auto (const Value&) -> BoolOrNone;

class UnaryBooleanOperator {
    const char* repr_;
    UBoolFn& fn_;

public:
    constexpr UnaryBooleanOperator(const char* r, UBoolFn* fn) :
        repr_(r),
        fn_(*fn)
    {}

    auto repr(ostream& o) const -> void {
        o << repr_;
    }

    auto eval(Expression& e, const Env& env) const -> BoolOrNone {
        return fn_(e.eval(env));
    }
};

template <> struct has_repr<UnaryBooleanOperator> {static constexpr bool value = true;};

using ArithFn = auto (Value, Value) -> Value;

class ArithmeticOperator {
    const char* repr_;
    ArithFn& fn_;

public:
    constexpr ArithmeticOperator(const char* r, ArithFn* fn) :
        repr_(r),
        fn_(*fn)
    {}

    auto repr(ostream& o) const -> void {
        o << repr_;
    }

    auto eval(Expression& e1, Expression& e2, const Env& env) const -> Value {
        return fn_(e1.eval(env), e2.eval(env));
    }
};

using UArithFn = auto (const Value&) -> Value;

template <> struct has_repr<ArithmeticOperator> {static constexpr bool value = true;};

class UnaryArithmeticOperator {
    const char* repr_;
    UArithFn& fn_;

public:
    constexpr UnaryArithmeticOperator(const char* r, UArithFn* fn) :
        repr_(r),
        fn_(*fn)
    {}

    auto repr(ostream& o) const -> void {
        o << repr_;
    }

    auto eval(Expression& e, const Env& env) const -> Value {
        return fn_(e.eval(env));
    }
};

template <> struct has_repr<UnaryArithmeticOperator> {static constexpr bool value = true;};

////////////////////////////////////////////////////

// Some operators...

constexpr auto eqOp   = ComparisonOperator{"==", operator==};
constexpr auto neqOp  = ComparisonOperator{"!=", operator!=};
constexpr auto lsOp   = ComparisonOperator{"<",  operator<};
constexpr auto grOp   = ComparisonOperator{">",  operator>};
constexpr auto lseqOp = ComparisonOperator{"<=", operator<=};
constexpr auto greqOp = ComparisonOperator{">=", operator>=};

constexpr auto isNullOp    = UnaryBooleanOperator{"IsNull",
    [](const Value& v){return BoolOrNone(unknown(v));}};
constexpr auto isNonNullOp = UnaryBooleanOperator{"IsNonNull",
    [](const Value& v){return BoolOrNone(!unknown(v));}};
constexpr auto notOp       = UnaryBooleanOperator{"NOT", operator!};

constexpr auto add  = ArithmeticOperator{"+", operator+};
constexpr auto sub  = ArithmeticOperator{"-", operator-};
constexpr auto mult = ArithmeticOperator{"*", operator*};
constexpr auto div  = ArithmeticOperator{"/", operator/};

constexpr auto negate = UnaryArithmeticOperator{"-", operator-};

////////////////////////////////////////////////////

// Expressions...

Expression::~Expression() noexcept = default;

class ValueExpression : public Expression {
public:
  ~ValueExpression() noexcept override = default;
  auto repr(ostream&) const -> void override = 0;
  auto eval(const Env&) const -> Value override = 0;
  auto eval_bool(const Env& env) const -> BoolOrNone override{
    return eval(env);
  }
};

class BoolExpression : public ValueExpression {
public:
  ~BoolExpression() noexcept override = default;
  auto repr(ostream&) const -> void override = 0;
  auto eval_bool(const Env&) const -> BoolOrNone override = 0;
  auto eval(const Env& env) const -> Value override {
    return eval_bool(env);
  }
};

// Boolean Expression types...

class ComparisonExpression : public BoolExpression {
    const ComparisonOperator& op;
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    ComparisonExpression(const ComparisonOperator& o, unique_ptr<ValueExpression> e, unique_ptr<ValueExpression> e_):
        op(o),
        e1(std::move(e)),
        e2(std::move(e_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << "(" << *e1 << op << *e2 << ")";
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        return op.eval(*e1, *e2, env);
    }
};

class OrExpression : public BoolExpression {
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    OrExpression(unique_ptr<ValueExpression> e, unique_ptr<ValueExpression> e_):
        e1(std::move(e)),
        e2(std::move(e_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << "(" << *e1 << " OR " << *e2 << ")";
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        BoolOrNone bn1(e1->eval_bool(env));
        if (bn1==BN_TRUE) return BN_TRUE;
        BoolOrNone bn2(e2->eval_bool(env));
        if (bn2==BN_TRUE) return BN_TRUE;
        if (bn1==BN_FALSE && bn2==BN_FALSE) return BN_FALSE;
        else return BN_UNKNOWN;
    }
};

class AndExpression : public BoolExpression {
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    AndExpression(unique_ptr<ValueExpression> e, unique_ptr<ValueExpression> e_):
        e1(std::move(e)),
        e2(std::move(e_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << "(" << *e1 << " AND " << *e2 << ")";
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        BoolOrNone bn1(e1->eval_bool(env));
        if (bn1==BN_FALSE) return BN_FALSE;
        BoolOrNone bn2(e2->eval_bool(env));
        if (bn2==BN_FALSE) return BN_FALSE;
        if (bn1==BN_TRUE && bn2==BN_TRUE) return BN_TRUE;
        else return BN_UNKNOWN;
    }
};

class UnaryBooleanExpression : public BoolExpression {
    const UnaryBooleanOperator& op;
    unique_ptr<ValueExpression> e1;

public:
    UnaryBooleanExpression(const UnaryBooleanOperator& o, unique_ptr<ValueExpression> e) :
        op(o),
        e1(std::move(e))
    {}

    auto repr(ostream& os) const -> void override final {
        os << op << "(" << *e1 << ")";
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        return op.eval(*e1, env);
    }
};

class LikeExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    string reString;
    std::regex regexBuffer;

    static string toRegex(const string& s, const string& escape) {
        string regex("^");
        if (escape.size()>1) throw std::logic_error("Internal error");
        char e = 0;
        if (escape.size()==1) {
            e = escape[0];
        }
        // Translate % -> .*, _ -> ., . ->\. *->\*
        bool doEscape = false;
        for (auto& i : s) {
            if ( e!=0 && i==e ) {
                doEscape = true;
                continue;
            }
            switch(i) {
                case '%':
                    if (doEscape) regex += i;
                    else regex += ".*";
                    break;
                case '_':
                    if (doEscape) regex += i;
                    else regex += ".";
                    break;
                case ']':
                case '{':
                case '}':
                case '(':
                case ')':
                case '-':
                case '+':
                case '?':
                case '|':
                    regex += '[';
                    regex += i;
                    regex += ']';
                    break;
                // Don't add any more cases here: these are sufficient,
                // adding more might turn on inadvertent matching
                case '\\':
                case '^':
                case '$':
                case '.':
                case '*':
                case '[':
                    regex += "\\";
                    // Fallthrough
                default:
                    regex += i;
                    break;
            }
            doEscape = false;
        }
        regex += "$";
        return regex;
    }

public:
    LikeExpression(unique_ptr<ValueExpression> e_, const string& like, const string& escape="")
    try :
        e(std::move(e_)),
        reString(toRegex(like, escape)),
        regexBuffer(reString, std::regex::basic)
    {}
    catch (std::regex_error& e) {
        ostringstream o("Regex Internal error: code=");
        o << e.code() << std::ends;
        throw std::logic_error(o.str());
    }

    auto repr(ostream& os) const -> void override final {
        os << *e << " REGEX_MATCH '" << reString << "'";
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        Value v(e->eval(env));
        if ( v.type()!=Value::T_STRING ) return BN_UNKNOWN;
        auto sv = std::get<string_view>(v.value);
        return BoolOrNone(std::regex_match(sv.cbegin(), sv.cend(), regexBuffer));
    }
};

class BetweenExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    unique_ptr<ValueExpression> l;
    unique_ptr<ValueExpression> u;

public:
    BetweenExpression(unique_ptr<ValueExpression> e_, unique_ptr<ValueExpression> l_, unique_ptr<ValueExpression> u_) :
        e(std::move(e_)),
        l(std::move(l_)),
        u(std::move(u_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << *e << " BETWEEN " << *l << " AND " << *u;
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        Value ve(e->eval(env));
        Value vl(l->eval(env));
        Value vu(u->eval(env));
        if (unknown(ve) || unknown(vl) || unknown(vu)) return BN_UNKNOWN;
        return BoolOrNone(ve>=vl && ve<=vu);
    }
};

class InExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    vector<unique_ptr<ValueExpression>> l;

public:
    InExpression(unique_ptr<ValueExpression> e_, vector<unique_ptr<ValueExpression>>&& l_) :
        e(std::move(e_)),
        l(std::move(l_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << *e << " IN (";
        for (std::size_t i = 0; i<l.size(); ++i){
            os << *l[i] << (i<l.size()-1 ? ", " : ")");
        }
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        Value ve(e->eval(env));
        if (unknown(ve)) return BN_UNKNOWN;
        BoolOrNone r = BN_FALSE;
        for (auto& le : l){
            Value li(le->eval(env));
            if (unknown(li)) {
                r = BN_UNKNOWN;
                continue;
            }
            if (ve==li) return BN_TRUE;
        }
        return r;
    }
};

class NotInExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    vector<unique_ptr<ValueExpression>> l;

public:
    NotInExpression(unique_ptr<ValueExpression> e_, vector<unique_ptr<ValueExpression>>&& l_) :
        e(std::move(e_)),
        l(std::move(l_))
    {}

    auto repr(ostream& os) const -> void override final {
        os << *e << " NOT IN (";
        for (std::size_t i = 0; i<l.size(); ++i){
            os << *l[i] << (i<l.size()-1 ? ", " : ")");
        }
    }

    auto eval_bool(const Env& env) const -> BoolOrNone override final {
        Value ve(e->eval(env));
        if (unknown(ve)) return BN_UNKNOWN;
        BoolOrNone r = BN_TRUE;
        for (auto& le : l){
            Value li(le->eval(env));
            if (unknown(li)) {
                r = BN_UNKNOWN;
                continue;
            }
            // Check if types are incompatible. If nothing further in the list
            // matches or is unknown and we had a type incompatibility then
            // result still false.
            if (r!=BN_UNKNOWN &&
                !sameType(ve,li) && !(numeric(ve) && numeric(li))) {
                r = BN_FALSE;
                continue;
            }

            if (ve==li) return BN_FALSE;
        }
        return r;
    }
};

// Arithmetic Expression types

class ArithmeticExpression : public ValueExpression {
    const ArithmeticOperator& op;
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    ArithmeticExpression(const ArithmeticOperator& o, unique_ptr<ValueExpression> e, unique_ptr<ValueExpression> e_):
        op(o),
        e1(std::move(e)),
        e2(std::move(e_))
    {}

    auto repr(ostream& os) const -> void override final{
        os << "(" << *e1 << op << *e2 << ")";
    }

    auto eval(const Env& env) const -> Value override final {
        return op.eval(*e1, *e2, env);
    }
};

class UnaryArithExpression : public ValueExpression {
    const UnaryArithmeticOperator& op;
    unique_ptr<ValueExpression> e1;

public:
    UnaryArithExpression(const UnaryArithmeticOperator& o, unique_ptr<ValueExpression> e) :
        op(o),
        e1(std::move(e))
    {}

    auto repr(ostream& os) const -> void override final {
        os << op << "(" << *e1 << ")";
    }

    auto eval(const Env& env) const -> Value override final {
        return op.eval(*e1, env);
    }
};

// Expression types...

class Literal : public ValueExpression {
    const Value value;

public:
    template <typename T>
    Literal(const T& v) :
        value(v)
    {}

    auto repr(ostream& os) const -> void override final {
        os << value;
    }

    auto eval(const Env&) const -> Value override final {
        return value;
    }
};

class StringLiteral : public ValueExpression {
    const string value;

public:
    StringLiteral(string_view v) :
        value(v)
    {}

    auto repr(ostream& os) const -> void override final {
        os << "'" << value << "'";
    }

    auto eval(const Env&) const -> Value override final {
        return string_view{value};
    }
};

class Identifier : public ValueExpression {
    string identifier;

public:
    Identifier(const string& i) :
        identifier(i)
    {}

    auto repr(ostream& os) const -> void override final {
        os << "I:" << identifier;
    }

    auto eval(const Env& env) const -> Value override final {
        return env.value(identifier);
    }
};

////////////////////////////////////////////////////

struct Parse {

[[noreturn]]
static inline
auto throwParseError(const Token& token, const string& msg) -> void {
    string error("Illegal selector: '");
    error += token.val;
    error += "': ";
    error += msg;
    throw std::range_error(error);
}

[[noreturn]]
static inline
auto throwParseError(Tokeniser& tokeniser, const string& msg) -> void {
    tokeniser.returnTokens();
    throwParseError(tokeniser.nextToken(), msg);
}

static
auto selectorExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    if ( tokeniser.nextToken().type==T_EOS ) {
        return make_unique<Literal>(true);
    }
    tokeniser.returnTokens();
    auto e = orExpression(tokeniser);
    if (tokeniser.nextToken().type != T_EOS) {
      throwParseError(tokeniser, "extra input");
    }
    return e;
}

static
auto orExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    auto e = andExpression(tokeniser);
    while ( tokeniser.nextToken().type==T_OR ) {
        e = make_unique<OrExpression>(std::move(e), andExpression(tokeniser));
    }
    tokeniser.returnTokens();
    return e;
}

static
auto andExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    auto e = comparisonExpression(tokeniser);
    while ( tokeniser.nextToken().type==T_AND ) {
        e = make_unique<AndExpression>(std::move(e), comparisonExpression(tokeniser));
    }
    tokeniser.returnTokens();
    return e;
}

static
auto conditionalNegate(bool negated, unique_ptr<BoolExpression> e) -> unique_ptr<BoolExpression>
{
    return negated ? make_unique<UnaryBooleanExpression>(notOp, std::move(e)) : std::move(e);
}

static
auto specialComparisons(Tokeniser& tokeniser, unique_ptr<ValueExpression> e1, bool negated = false)  -> unique_ptr<BoolExpression>
{
    switch (tokeniser.nextToken().type) {
    case T_LIKE: {
        auto t = tokeniser.nextToken();
        if ( t.type!=T_STRING ) {
            throwParseError(tokeniser, "expected string after LIKE");
        }
        // Check for "ESCAPE"
        if ( tokeniser.nextToken().type==T_ESCAPE ) {
            auto e = tokeniser.nextToken();
            if ( e.type!=T_STRING ) {
                throwParseError(tokeniser, "expected string after ESCAPE");
            }
            if (e.val.size()>1) {
                throwParseError(tokeniser, "single character string required after ESCAPE");
            }
            if (e.val=="%" || e.val=="_") {
                throwParseError(tokeniser, "'%' and '_' are not allowed as ESCAPE characters");
            }
            return conditionalNegate(negated, make_unique<LikeExpression>(std::move(e1), t.val, e.val));
        } else {
            tokeniser.returnTokens();
            return conditionalNegate(negated, make_unique<LikeExpression>(std::move(e1), t.val));
        }
    }
    case T_BETWEEN: {
        auto lower = addExpression(tokeniser);
        if ( tokeniser.nextToken().type!=T_AND ) {
            throwParseError(tokeniser, "expected AND after BETWEEN");
        }
        return conditionalNegate(negated, make_unique<BetweenExpression>(std::move(e1), std::move(lower), addExpression(tokeniser)));
    }
    case T_IN: {
        if ( tokeniser.nextToken().type!=T_LPAREN ) {
            throwParseError(tokeniser, "missing '(' after IN");
        }
        vector<unique_ptr<ValueExpression>> list;
        do {
            list.push_back(addExpression(tokeniser));
        } while (tokeniser.nextToken().type==T_COMMA);
        tokeniser.returnTokens();
        if ( tokeniser.nextToken().type!=T_RPAREN ) {
            throwParseError(tokeniser, "missing ',' or ')' after IN");
        }
        if (negated) return make_unique<NotInExpression>(std::move(e1), std::move(list));
        else return make_unique<InExpression>(std::move(e1), std::move(list));
    }
    default:
        throwParseError(tokeniser, "expected LIKE, IN or BETWEEN");
    }
}

static
auto comparisonExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    if ( tokeniser.nextToken().type==T_NOT ) {
        return make_unique<UnaryBooleanExpression>(notOp, comparisonExpression(tokeniser));
    }

    tokeniser.returnTokens();
    auto e1 = addExpression(tokeniser);

    const ComparisonOperator* op;
    switch (tokeniser.nextToken().type) {
    // Check for "IS NULL" and "IS NOT NULL"
    case T_IS:
        // The rest must be T_NULL or T_NOT, T_NULL
        switch (tokeniser.nextToken().type) {
        case T_NULL:
            return make_unique<UnaryBooleanExpression>(isNullOp, std::move(e1));
        case T_NOT:
            if ( tokeniser.nextToken().type == T_NULL)
                return make_unique<UnaryBooleanExpression>(isNonNullOp, std::move(e1));
        default:
            throwParseError(tokeniser, "expected NULL or NOT NULL after IS");
        }
    case T_NOT:
        return specialComparisons(tokeniser, std::move(e1), true);
    case T_BETWEEN:
    case T_LIKE:
    case T_IN:
        tokeniser.returnTokens();
        return specialComparisons(tokeniser, std::move(e1));
    case T_EQUAL: op = &eqOp; break;
    case T_NEQ:   op = &neqOp; break;
    case T_LESS:  op = &lsOp; break;
    case T_GRT:   op = &grOp; break;
    case T_LSEQ:  op = &lseqOp; break;
    case T_GREQ:  op = &greqOp; break;
    default:
        tokeniser.returnTokens();
        return e1;
    }
    return make_unique<ComparisonExpression>(*op, std::move(e1), addExpression(tokeniser));
}

static
auto addExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    auto e = multiplyExpression(tokeniser);

    auto t = tokeniser.nextToken();
    while (t.type==T_PLUS || t.type==T_MINUS ) {
        const ArithmeticOperator& op = t.type==T_PLUS ? add : sub;
        e = make_unique<ArithmeticExpression>(op, std::move(e), multiplyExpression(tokeniser));
        t = tokeniser.nextToken();
    }

    tokeniser.returnTokens();
    return e;
}

static
auto multiplyExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    auto e = unaryArithExpression(tokeniser);

    auto t = tokeniser.nextToken();
    while (t.type==T_MULT || t.type==T_DIV ) {
        const ArithmeticOperator& op = t.type==T_MULT ? mult : div;
        e = make_unique<ArithmeticExpression>(op, std::move(e), unaryArithExpression(tokeniser));
        t = tokeniser.nextToken();
    }

    tokeniser.returnTokens();
    return e;
}

static
auto exactNumeric(const Token& token, bool negate) -> unique_ptr<ValueExpression>
{
    int base = 0;
    string s;
    std::remove_copy(token.val.begin(), token.val.end(), std::back_inserter(s), '_');
    if (s[1]=='b' || s[1]=='B') {
        base = 2;
        s = s.substr(2);
    } else if (s[1]=='x' || s[1]=='X') {
        base = 16;
        s = s.substr(2);
    } if (s[0]=='0') {
        base = 8;
    }
    errno = 0;
    uint64_t value = strtoull(s.c_str(), 0, base);
    if (!errno && (base || value<=INT64_MAX)) {
        int64_t r = value;
        return make_unique<Literal>((negate ? -r : r));
    }
    if (negate && value==INT64_MAX+1ull) return make_unique<Literal>(INT64_MIN);
    throwParseError(token, "integer literal too big");
}

static
auto approxNumeric(const Token& token) -> unique_ptr<ValueExpression>
{
    errno = 0;
    string s;
    std::remove_copy(token.val.begin(), token.val.end(), std::back_inserter(s), '_');
    double value = std::strtod(s.c_str(), 0);
    if (!errno) return make_unique<Literal>(value);
    throwParseError(token, "floating literal overflow/underflow");
}

static
auto unaryArithExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    switch (tokeniser.nextToken().type) {
    case T_LPAREN: {
        auto e = orExpression(tokeniser);
        if ( tokeniser.nextToken().type!=T_RPAREN ) {
            throwParseError(tokeniser, "missing ')' after '('");
        }
        return e;
    }
    case T_PLUS:
        break; // Unary + is no op
    case T_MINUS: {
        auto t = tokeniser.nextToken();
        // Special case for negative numerics
        if (t.type==T_NUMERIC_EXACT) {
            return exactNumeric(t, true);
        } else {
            tokeniser.returnTokens();
            return make_unique<UnaryArithExpression>(negate, unaryArithExpression(tokeniser));
        }
    }
    default:
        tokeniser.returnTokens();
        break;
    }

    return primaryExpression(tokeniser);
}

static
auto primaryExpression(Tokeniser& tokeniser) -> unique_ptr<ValueExpression>
{
    auto t = tokeniser.nextToken();
    switch (t.type) {
        case T_IDENTIFIER:
            return make_unique<Identifier>(t.val);
        case T_STRING:
            return make_unique<StringLiteral>(t.val);
        case T_FALSE:
            return make_unique<Literal>(false);
        case T_TRUE:
            return make_unique<Literal>(true);
        case T_NUMERIC_EXACT:
            return exactNumeric(t, false);
        case T_NUMERIC_APPROX:
            return approxNumeric(t);
        default:
            throwParseError(tokeniser, "expected literal or identifier");
    }
}

};

///////////////////////////////////////////////////////////

// Top level parser
auto make_selector(string_view exp) -> unique_ptr<Expression>
{
    auto tokeniser = Tokeniser{exp};
    return Parse::selectorExpression(tokeniser);
}

auto eval(const Expression& exp, const Env& env) -> bool
{
    return exp.eval_bool(env)==BN_TRUE;
}

auto operator<<(std::ostream& o, const Expression& e) -> std::ostream&
{
    e.repr(o);
    return o;
}

}
