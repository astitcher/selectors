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

#include "SelectorEnv.h"
#include "SelectorToken.h"
#include "SelectorValue.h"

#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <regex>
#include <string>
#include <memory>
#include <ostream>

#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

using std::unique_ptr;
using std::make_unique;

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


using std::string;
using std::ostream;

namespace selector {

class ValueExpression {
public:
    virtual ~ValueExpression() {}
    virtual void repr(std::ostream&) const = 0;
    virtual Value eval(const Env&) const = 0;

    virtual BoolOrNone eval_bool(const Env& env) const {
        Value v = eval(env);
        if (v.type==Value::T_BOOL) return BoolOrNone(v.b);
        else return BN_UNKNOWN;
    }
};

class BoolExpression : public ValueExpression {
public:
    virtual ~BoolExpression() {}
    virtual void repr(std::ostream&) const = 0;
    virtual BoolOrNone eval_bool(const Env&) const = 0;

    Value eval(const Env& env) const {
        return eval_bool(env);
    }
};

// Operators

class ComparisonOperator {
public:
    virtual ~ComparisonOperator() {}
    virtual void repr(ostream&) const = 0;
    virtual BoolOrNone eval(ValueExpression&, ValueExpression&, const Env&) const = 0;
};

class UnaryBooleanOperator {
public:
    virtual ~UnaryBooleanOperator() {}
    virtual void repr(ostream&) const = 0;
    virtual BoolOrNone eval(ValueExpression&, const Env&) const = 0;
};

class ArithmeticOperator {
public:
    virtual ~ArithmeticOperator() {}
    virtual void repr(ostream&) const = 0;
    virtual Value eval(ValueExpression&, ValueExpression&, const Env&) const = 0;
};

class UnaryArithmeticOperator {
public:
    virtual ~UnaryArithmeticOperator() {}
    virtual void repr(ostream&) const = 0;
    virtual Value eval(ValueExpression&, const Env&) const = 0;
};

////////////////////////////////////////////////////

// Convenience outputters

ostream& operator<<(ostream& os, const ValueExpression& e)
{
    e.repr(os);
    return os;
}

ostream& operator<<(ostream& os, const ComparisonOperator& e)
{
    e.repr(os);
    return os;
}

ostream& operator<<(ostream& os, const UnaryBooleanOperator& e)
{
    e.repr(os);
    return os;
}

ostream& operator<<(ostream& os, const ArithmeticOperator& e)
{
    e.repr(os);
    return os;
}

ostream& operator<<(ostream& os, const UnaryArithmeticOperator& e)
{
    e.repr(os);
    return os;
}

// Boolean Expression types...

class ComparisonExpression : public BoolExpression {
    ComparisonOperator* op;
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    ComparisonExpression(ComparisonOperator* o, ValueExpression* e, ValueExpression* e_):
        op(o),
        e1(e),
        e2(e_)
    {}

    void repr(ostream& os) const {
        os << "(" << *e1 << *op << *e2 << ")";
    }

    BoolOrNone eval_bool(const Env& env) const {
        return op->eval(*e1, *e2, env);
    }
};

class OrExpression : public BoolExpression {
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    OrExpression(ValueExpression* e, ValueExpression* e_):
        e1(e),
        e2(e_)
    {}

    void repr(ostream& os) const {
        os << "(" << *e1 << " OR " << *e2 << ")";
    }

    BoolOrNone eval_bool(const Env& env) const {
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
    AndExpression(ValueExpression* e, ValueExpression* e_):
        e1(e),
        e2(e_)
    {}

    void repr(ostream& os) const {
        os << "(" << *e1 << " AND " << *e2 << ")";
    }

    BoolOrNone eval_bool(const Env& env) const {
        BoolOrNone bn1(e1->eval_bool(env));
        if (bn1==BN_FALSE) return BN_FALSE;
        BoolOrNone bn2(e2->eval_bool(env));
        if (bn2==BN_FALSE) return BN_FALSE;
        if (bn1==BN_TRUE && bn2==BN_TRUE) return BN_TRUE;
        else return BN_UNKNOWN;
    }
};

class UnaryBooleanExpression : public BoolExpression {
    UnaryBooleanOperator* op;
    unique_ptr<ValueExpression> e1;

public:
    UnaryBooleanExpression(UnaryBooleanOperator* o, ValueExpression* e) :
        op(o),
        e1(e)
    {}

    void repr(ostream& os) const {
        os << *op << "(" << *e1 << ")";
    }

    BoolOrNone eval_bool(const Env& env) const {
        return op->eval(*e1, env);
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
        for (string::const_iterator i = s.begin(); i!=s.end(); ++i) {
            if ( e!=0 && *i==e ) {
                doEscape = true;
                continue;
            }
            switch(*i) {
                case '%':
                    if (doEscape) regex += *i;
                    else regex += ".*";
                    break;
                case '_':
                    if (doEscape) regex += *i;
                    else regex += ".";
                    break;
                case ']':
                    regex += "[]]";
                    break;
                case '-':
                    regex += "[-]";
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
                    regex += *i;
                    break;
            }
            doEscape = false;
        }
        regex += "$";
        return regex;
    }

public:
    LikeExpression(ValueExpression* e_, const string& like, const string& escape="") :
        e(e_),
        reString(toRegex(like, escape)),
        regexBuffer(reString)
    {}

    void repr(ostream& os) const {
        os << *e << " REGEX_MATCH '" << reString << "'";
    }

    BoolOrNone eval_bool(const Env& env) const {
        Value v(e->eval(env));
        if ( v.type!=Value::T_STRING ) return BN_UNKNOWN;
        return BoolOrNone(std::regex_match(*v.s, regexBuffer));
    }
};

class BetweenExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    unique_ptr<ValueExpression> l;
    unique_ptr<ValueExpression> u;

public:
    BetweenExpression(ValueExpression* e_, ValueExpression* l_, ValueExpression* u_) :
        e(e_),
        l(l_),
        u(u_)
    {}

    void repr(ostream& os) const {
        os << *e << " BETWEEN " << *l << " AND " << *u;
    }

    BoolOrNone eval_bool(const Env& env) const {
        Value ve(e->eval(env));
        Value vl(l->eval(env));
        Value vu(u->eval(env));
        if (unknown(ve) || unknown(vl) || unknown(vu)) return BN_UNKNOWN;
        return BoolOrNone(ve>=vl && ve<=vu);
    }
};

class InExpression : public BoolExpression {
    unique_ptr<ValueExpression> e;
    boost::ptr_vector<ValueExpression> l;

public:
    InExpression(ValueExpression* e_, boost::ptr_vector<ValueExpression>& l_) :
        e(e_)
    {
        l.swap(l_);
    }

    void repr(ostream& os) const {
        os << *e << " IN (";
        for (std::size_t i = 0; i<l.size(); ++i){
            os << l[i] << (i<l.size()-1 ? ", " : ")");
        }
    }

    BoolOrNone eval_bool(const Env& env) const {
        Value ve(e->eval(env));
        if (unknown(ve)) return BN_UNKNOWN;
        BoolOrNone r = BN_FALSE;
        for (std::size_t i = 0; i<l.size(); ++i){
            Value li(l[i].eval(env));
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
    boost::ptr_vector<ValueExpression> l;

public:
    NotInExpression(ValueExpression* e_, boost::ptr_vector<ValueExpression>& l_) :
        e(e_)
    {
        l.swap(l_);
    }

    void repr(ostream& os) const {
        os << *e << " NOT IN (";
        for (std::size_t i = 0; i<l.size(); ++i){
            os << l[i] << (i<l.size()-1 ? ", " : ")");
        }
    }

    BoolOrNone eval_bool(const Env& env) const {
        Value ve(e->eval(env));
        if (unknown(ve)) return BN_UNKNOWN;
        BoolOrNone r = BN_TRUE;
        for (std::size_t i = 0; i<l.size(); ++i){
            Value li(l[i].eval(env));
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
    ArithmeticOperator* op;
    unique_ptr<ValueExpression> e1;
    unique_ptr<ValueExpression> e2;

public:
    ArithmeticExpression(ArithmeticOperator* o, ValueExpression* e, ValueExpression* e_):
        op(o),
        e1(e),
        e2(e_)
    {}

    void repr(ostream& os) const {
        os << "(" << *e1 << *op << *e2 << ")";
    }

    Value eval(const Env& env) const {
        return op->eval(*e1, *e2, env);
    }
};

class UnaryArithExpression : public ValueExpression {
    UnaryArithmeticOperator* op;
    unique_ptr<ValueExpression> e1;

public:
    UnaryArithExpression(UnaryArithmeticOperator* o, ValueExpression* e) :
        op(o),
        e1(e)
    {}

    void repr(ostream& os) const {
        os << *op << "(" << *e1 << ")";
    }

    Value eval(const Env& env) const {
        return op->eval(*e1, env);
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

    void repr(ostream& os) const {
        os << value;
    }

    Value eval(const Env&) const {
        return value;
    }
};

class StringLiteral : public ValueExpression {
    const string value;

public:
    StringLiteral(const string& v) :
        value(v)
    {}

    void repr(ostream& os) const {
        os << "'" << value << "'";
    }

    Value eval(const Env&) const {
        return value;
    }
};

class Identifier : public ValueExpression {
    string identifier;

public:
    Identifier(const string& i) :
        identifier(i)
    {}

    void repr(ostream& os) const {
        os << "I:" << identifier;
    }

    Value eval(const Env& env) const {
        return env.value(identifier);
    }
};

////////////////////////////////////////////////////

// Some operators...

typedef bool BoolOp(const Value&, const Value&);

BoolOrNone booleval(BoolOp* op, ValueExpression& e1, ValueExpression& e2, const Env& env) {
    const Value v1(e1.eval(env));
    if (!unknown(v1)) {
        const Value v2(e2.eval(env));
        if (!unknown(v2)) {
            return BoolOrNone(op(v1, v2));
        }
    }
    return BN_UNKNOWN;
}

// "="
class Eq : public ComparisonOperator {
    void repr(ostream& os) const {
        os << "=";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator==, e1, e2, env);
    }
};

// "<>"
class Neq : public ComparisonOperator {
    void repr(ostream& os) const {
        os << "<>";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator!=, e1, e2, env);
    }
};

// "<"
class Ls : public ComparisonOperator {
    void repr(ostream& os) const {
        os << "<";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator<, e1, e2, env);
    }
};

// ">"
class Gr : public ComparisonOperator {
    void repr(ostream& os) const {
        os << ">";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator>, e1, e2, env);
    }
};

// "<="
class Lseq : public ComparisonOperator {
    void repr(ostream& os) const {
        os << "<=";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator<=, e1, e2, env);
    }
};

// ">="
class Greq : public ComparisonOperator {
    void repr(ostream& os) const {
        os << ">=";
    }

    BoolOrNone eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return booleval(&operator>=, e1, e2, env);
    }
};

// "IS NULL"
class IsNull : public UnaryBooleanOperator {
    void repr(ostream& os) const {
        os << "IsNull";
    }

    BoolOrNone eval(ValueExpression& e, const Env& env) const {
        return BoolOrNone(unknown(e.eval(env)));
    }
};

// "IS NOT NULL"
class IsNonNull : public UnaryBooleanOperator {
    void repr(ostream& os) const {
        os << "IsNonNull";
    }

    BoolOrNone eval(ValueExpression& e, const Env& env) const {
        return BoolOrNone(!unknown(e.eval(env)));
    }
};

// "NOT"
class Not : public UnaryBooleanOperator {
    void repr(ostream& os) const {
        os << "NOT";
    }

    BoolOrNone eval(ValueExpression& e, const Env& env) const {
        BoolOrNone bn = e.eval_bool(env);
        if (bn==BN_UNKNOWN) return bn;
        else return BoolOrNone(!bn);
    }
};

class Negate : public UnaryArithmeticOperator {
    void repr(ostream& os) const {
        os << "-";
    }

    Value eval(ValueExpression& e, const Env& env) const {
        return -e.eval(env);
    }
};

class Add : public ArithmeticOperator {
    void repr(ostream& os) const {
        os << "+";
    }

    Value eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return e1.eval(env)+e2.eval(env);
    }
};

class Sub : public ArithmeticOperator {
    void repr(ostream& os) const {
        os << "-";
    }

    Value eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return e1.eval(env)-e2.eval(env);
    }
};

class Mult : public ArithmeticOperator {
    void repr(ostream& os) const {
        os << "*";
    }

    Value eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return e1.eval(env)*e2.eval(env);
    }
};

class Div : public ArithmeticOperator {
    void repr(ostream& os) const {
        os << "/";
    }

    Value eval(ValueExpression& e1, ValueExpression& e2, const Env& env) const {
        return e1.eval(env)/e2.eval(env);
    }
};

Eq eqOp;
Neq neqOp;
Ls lsOp;
Gr grOp;
Lseq lseqOp;
Greq greqOp;
IsNull isNullOp;
IsNonNull isNonNullOp;
Not notOp;

Negate negate;
Add add;
Sub sub;
Mult mult;
Div div;

////////////////////////////////////////////////////

void throwParseError(Tokeniser& tokeniser, const string& msg) {
    tokeniser.returnTokens();
    string error("Illegal selector: '");
    error += tokeniser.nextToken().val;
    error += "': ";
    error += msg;
    throw std::range_error(error);
}

class Parse {

friend unique_ptr<Expression> make_selector(const string&);

string error;

unique_ptr<ValueExpression> selectorExpression(Tokeniser& tokeniser)
{
    if ( tokeniser.nextToken().type==T_EOS ) {
        return make_unique<Literal>(true);
    }
    tokeniser.returnTokens();
    return orExpression(tokeniser);
}

unique_ptr<ValueExpression> orExpression(Tokeniser& tokeniser)
{
    unique_ptr<ValueExpression> e(andExpression(tokeniser));
    if (!e.get()) return 0;
    while ( tokeniser.nextToken().type==T_OR ) {
        unique_ptr<ValueExpression> e1(e.release());
        unique_ptr<ValueExpression> e2(andExpression(tokeniser));
        if (!e2.get()) return 0;
        e = make_unique<OrExpression>(e1.release(), e2.release());
    }
    tokeniser.returnTokens();
    return e;
}

unique_ptr<ValueExpression> andExpression(Tokeniser& tokeniser)
{
    unique_ptr<ValueExpression> e(comparisonExpression(tokeniser));
    if (!e.get()) return 0;
    while ( tokeniser.nextToken().type==T_AND ) {
        unique_ptr<ValueExpression> e1(e.release());
        unique_ptr<ValueExpression> e2(comparisonExpression(tokeniser));
        if (!e2.get()) return 0;
        e = make_unique<AndExpression>(e1.release(), e2.release());
    }
    tokeniser.returnTokens();
    return e;
}

unique_ptr<BoolExpression> specialComparisons(Tokeniser& tokeniser, unique_ptr<ValueExpression> e1, bool negated = false) {
    switch (tokeniser.nextToken().type) {
    case T_LIKE: {
        const Token t = tokeniser.nextToken();
        if ( t.type!=T_STRING ) {
            error = "expected string after LIKE";
            return 0;
        }
        // Check for "ESCAPE"
        unique_ptr<BoolExpression> l;
        if ( tokeniser.nextToken().type==T_ESCAPE ) {
            const Token e = tokeniser.nextToken();
            if ( e.type!=T_STRING ) {
                error = "expected string after ESCAPE";
                return 0;
            }
            if (e.val.size()>1) {
                throwParseError(tokeniser, "single character string required after ESCAPE");
            }
            if (e.val=="%" || e.val=="_") {
                throwParseError(tokeniser, "'%' and '_' are not allowed as ESCAPE characters");
            }
            l = make_unique<LikeExpression>(e1.release(), t.val, e.val);
        } else {
            tokeniser.returnTokens();
            l = make_unique<LikeExpression>(e1.release(), t.val);
        }
        if (negated) return make_unique<UnaryBooleanExpression>(&notOp, l.release());
        else return l;
    }
    case T_BETWEEN: {
        unique_ptr<ValueExpression> lower(addExpression(tokeniser));
        if ( !lower.get() ) return 0;
        if ( tokeniser.nextToken().type!=T_AND ) {
            error = "expected AND after BETWEEN";
            return 0;
        }
        unique_ptr<ValueExpression> upper(addExpression(tokeniser));
        if ( !upper.get() ) return 0;
        auto b = make_unique<BetweenExpression>(e1.release(), lower.release(), upper.release());
        if (negated) return make_unique<UnaryBooleanExpression>(&notOp, b.release());
        else return b;
    }
    case T_IN: {
        if ( tokeniser.nextToken().type!=T_LPAREN ) {
            error = "missing '(' after IN";
            return 0;
        }
        boost::ptr_vector<ValueExpression> list;
        do {
            unique_ptr<ValueExpression> e(addExpression(tokeniser));
            if (!e.get()) return 0;
            list.push_back(e.release());
        } while (tokeniser.nextToken().type==T_COMMA);
        tokeniser.returnTokens();
        if ( tokeniser.nextToken().type!=T_RPAREN ) {
            error = "missing ',' or ')' after IN";
            return 0;
        }
        if (negated) return make_unique<NotInExpression>(e1.release(), list);
        else return make_unique<InExpression>(e1.release(), list);
    }
    default:
        error = "expected LIKE, IN or BETWEEN";
        return 0;
    }
}

unique_ptr<ValueExpression> comparisonExpression(Tokeniser& tokeniser)
{
    const Token t = tokeniser.nextToken();
    if ( t.type==T_NOT ) {
        unique_ptr<ValueExpression> e(comparisonExpression(tokeniser));
        if (!e.get()) return 0;
        return make_unique<UnaryBooleanExpression>(&notOp, e.release());
    }

    tokeniser.returnTokens();
    unique_ptr<ValueExpression> e1(addExpression(tokeniser));
    if (!e1.get()) return 0;

    switch (tokeniser.nextToken().type) {
    // Check for "IS NULL" and "IS NOT NULL"
    case T_IS:
        // The rest must be T_NULL or T_NOT, T_NULL
        switch (tokeniser.nextToken().type) {
            case T_NULL:
                return make_unique<UnaryBooleanExpression>(&isNullOp, e1.release());
            case T_NOT:
                if ( tokeniser.nextToken().type == T_NULL)
                    return make_unique<UnaryBooleanExpression>(&isNonNullOp, e1.release());
            default:
                error = "expected NULL or NOT NULL after IS";
                return 0;
        }
    case T_NOT: {
        return specialComparisons(tokeniser, std::move(e1), true);
    }
    case T_BETWEEN:
    case T_LIKE:
    case T_IN: {
        tokeniser.returnTokens();
        return specialComparisons(tokeniser, std::move(e1));
    }
    default:
        break;
    }
    tokeniser.returnTokens();

    ComparisonOperator* op;
    switch (tokeniser.nextToken().type) {
    case T_EQUAL: op = &eqOp; break;
    case T_NEQ: op = &neqOp; break;
    case T_LESS: op = &lsOp; break;
    case T_GRT: op = &grOp; break;
    case T_LSEQ: op = &lseqOp; break;
    case T_GREQ: op = &greqOp; break;
    default:
        tokeniser.returnTokens();
        return e1;
    }

    unique_ptr<ValueExpression> e2(addExpression(tokeniser));
    if (!e2.get()) return 0;

    return make_unique<ComparisonExpression>(op, e1.release(), e2.release());
}

unique_ptr<ValueExpression> addExpression(Tokeniser& tokeniser)
{
    unique_ptr<ValueExpression> e(multiplyExpression(tokeniser));
    if (!e.get()) return 0;

    Token t = tokeniser.nextToken();
    while (t.type==T_PLUS || t.type==T_MINUS ) {
        ArithmeticOperator* op;
        switch (t.type) {
        case T_PLUS: op = &add; break;
        case T_MINUS: op = &sub; break;
        default:
            error = "internal error processing binary + or -";
            return 0;
        }
        unique_ptr<ValueExpression> e1(e.release());
        unique_ptr<ValueExpression> e2(multiplyExpression(tokeniser));
        if (!e2.get()) return 0;
        e = make_unique<ArithmeticExpression>(op, e1.release(), e2.release());
        t = tokeniser.nextToken();
    }

    tokeniser.returnTokens();
    return e;
}

unique_ptr<ValueExpression> multiplyExpression(Tokeniser& tokeniser)
{
    unique_ptr<ValueExpression> e(unaryArithExpression(tokeniser));
    if (!e.get()) return 0;

    Token t = tokeniser.nextToken();
    while (t.type==T_MULT || t.type==T_DIV ) {
        ArithmeticOperator* op;
        switch (t.type) {
        case T_MULT: op = &mult; break;
        case T_DIV: op = &div; break;
        default:
            error = "internal error processing * or /";
            return 0;
        }
        unique_ptr<ValueExpression> e1(e.release());
        unique_ptr<ValueExpression> e2(unaryArithExpression(tokeniser));
        if (!e2.get()) return 0;
        e = make_unique<ArithmeticExpression>(op, e1.release(), e2.release());
        t = tokeniser.nextToken();
    }

    tokeniser.returnTokens();
    return e;
}

unique_ptr<ValueExpression> unaryArithExpression(Tokeniser& tokeniser)
{
    const Token t = tokeniser.nextToken();
    switch (t.type) {
    case T_LPAREN: {
        unique_ptr<ValueExpression> e(orExpression(tokeniser));
        if (!e.get()) return 0;
        if ( tokeniser.nextToken().type!=T_RPAREN ) {
            error = "missing ')' after '('";
            return 0;
        }
        return e;
    }
    case T_PLUS:
        break; // Unary + is no op
    case T_MINUS: {
        const Token t = tokeniser.nextToken();
        // Special case for negative numerics
        if (t.type==T_NUMERIC_EXACT) {
            unique_ptr<ValueExpression> e(parseExactNumeric(t, true));
            return e;
        } else {
            tokeniser.returnTokens();
            unique_ptr<ValueExpression> e(unaryArithExpression(tokeniser));
            if (!e.get()) return 0;
            return make_unique<UnaryArithExpression>(&negate, e.release());
        }
    }
    default:
        tokeniser.returnTokens();
        break;
    }

    unique_ptr<ValueExpression> e(primaryExpression(tokeniser));
    return e;
}

unique_ptr<ValueExpression> parseExactNumeric(const Token& token, bool negate)
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
    error = "integer literal too big";
    return 0;
}

unique_ptr<ValueExpression> parseApproxNumeric(const Token& token)
{
    errno = 0;
    string s;
    std::remove_copy(token.val.begin(), token.val.end(), std::back_inserter(s), '_');
    double value = std::strtod(s.c_str(), 0);
    if (!errno) return make_unique<Literal>(value);
    error = "floating literal overflow/underflow";
    return 0;
}

unique_ptr<ValueExpression> primaryExpression(Tokeniser& tokeniser
  
)
{
    const Token& t = tokeniser.nextToken();
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
            return parseExactNumeric(t, false);
        case T_NUMERIC_APPROX:
            return parseApproxNumeric(t);
        default:
            error = "expected literal or identifier";
            return 0;
    }
}

};

///////////////////////////////////////////////////////////

// Top level parser
class ConcreteExpression : public Expression {
    unique_ptr<ValueExpression> expression;

    void repr(ostream& os) const {
        expression->repr(os);
    }

    bool eval(const Env& env) const {
        BoolOrNone bn = expression->eval_bool(env);
        if (bn==BN_TRUE) return true;
        else return false;
    }

public:
    ConcreteExpression(unique_ptr<ValueExpression> be) :
        expression(std::move(be))
    {}
};


unique_ptr<Expression> make_selector(const string& exp)
{
    string::const_iterator s = exp.begin();
    string::const_iterator e = exp.end();
    Tokeniser tokeniser(s,e);
    Parse parse;
    auto b = make_unique<ConcreteExpression>(parse.selectorExpression(tokeniser));
    if (!b) {
        throwParseError(tokeniser, parse.error);
    }
    if (tokeniser.nextToken().type != T_EOS) {
        throwParseError(tokeniser, "extra input");
    }
    return b;
}

}
