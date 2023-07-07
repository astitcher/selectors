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

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using std::get;
using std::string;
using std::string_view;
using std::unordered_map;
using std::unique_ptr;
using std::vector;

using namespace std::literals;

namespace selector::tests {

typedef bool (*TokeniseF)(string_view&,Token&);

template <typename F>
bool tokeniserCheck(string_view& sv, Token& tok, F f) {
    Token t1;
    auto sv1 = sv;
    bool r = tokenise(sv1, t1);
    if (r && f(t1.type)) {tok = t1; sv = sv1; return true;}
    return false;
}

bool tokeniseEos(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t==selector::T_EOS;
        }
    );
}

bool tokeniseParens(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t==selector::T_LPAREN || t==selector::T_RPAREN;
        }
    );
}

bool tokeniseOperator(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t>=selector::T_PLUS && t<=selector::T_GREQ;
        }
    );
}

bool tokeniseString(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t==selector::T_STRING;
        }
    );
}

bool tokeniseIdentifier(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t==selector::T_IDENTIFIER;
        }
    );
}

bool tokeniseReservedWord(string_view& sv, Token& tok)
{
    auto sv1 = sv;
    Token t1;
    if (tokenise(sv1, t1)) {
    switch (t1.type) {
    case selector::T_AND:
    case selector::T_BETWEEN:
    case selector::T_ESCAPE:
    case selector::T_FALSE:
    case selector::T_IN:
    case selector::T_IS:
    case selector::T_LIKE:
    case selector::T_NOT:
    case selector::T_NULL:
    case selector::T_OR:
    case selector::T_TRUE:
        tok = t1;
        sv = sv1;
        return true;
    default:
        break;
    }
    }
    return false;
}

bool tokeniseNumeric(string_view& sv, Token& tok)
{
    return tokeniserCheck(
        sv, tok, 
        [](TokenType t) -> bool {
            return t==selector::T_NUMERIC_EXACT || t==selector::T_NUMERIC_APPROX;
        }
    );
}


void verifyTokeniserSuccess(TokeniseF t, const char* ss, TokenType tt, const char* tv, const char* fs) {
    Token tok;
    string s{ss};
    string_view sv{s};
    CHECK(t(sv, tok));
    CHECK(tok == Token(tt, tv));
    CHECK(string(sv) == fs);
}

void verifyTokeniserFail(TokeniseF t, const char* c) {
    Token tok;
    string s{c};
    string_view sv{s};
    CHECK(!t(sv, tok));
    CHECK(string(sv) == c);
}

TEST_CASE( "Selectors" ) {

SECTION("tokeniseSuccess")
{
    verifyTokeniserSuccess(&tokenise, "", selector::T_EOS, "", "");
    verifyTokeniserSuccess(&tokenise, " ", selector::T_EOS, "", "");
    verifyTokeniserSuccess(&tokenise, "null_123+blah", selector::T_IDENTIFIER, "null_123", "+blah");
    verifyTokeniserSuccess(&tokenise, "\"null-123\"+blah", selector::T_IDENTIFIER, "null-123", "+blah");
    verifyTokeniserSuccess(&tokenise, "\"This is an \"\"odd!\"\" identifier\"+blah", selector::T_IDENTIFIER, "This is an \"odd!\" identifier", "+blah");
    verifyTokeniserSuccess(&tokenise, "null+blah", selector::T_NULL, "null", "+blah");
    verifyTokeniserSuccess(&tokenise, "null+blah", selector::T_NULL, "null", "+blah");
    verifyTokeniserSuccess(&tokenise, "Is nOt null", selector::T_IS, "Is", " nOt null");
    verifyTokeniserSuccess(&tokenise, "nOt null", selector::T_NOT, "nOt", " null");
    verifyTokeniserSuccess(&tokenise, "Is nOt null", selector::T_IS, "Is", " nOt null");
    verifyTokeniserSuccess(&tokenise, "'Hello World'", selector::T_STRING, "Hello World", "");
    verifyTokeniserSuccess(&tokenise, "'Hello World''s end'a bit more", selector::T_STRING, "Hello World's end", "a bit more");
    verifyTokeniserSuccess(&tokenise, "=blah", selector::T_EQUAL, "=", "blah");
    verifyTokeniserSuccess(&tokenise, "<> Identifier", selector::T_NEQ, "<>", " Identifier");
    verifyTokeniserSuccess(&tokenise, "(a and b) not c", selector::T_LPAREN, "(", "a and b) not c");
    verifyTokeniserSuccess(&tokenise, ") not c", selector::T_RPAREN, ")", " not c");
    verifyTokeniserSuccess(&tokenise, "017kill", selector::T_NUMERIC_EXACT, "017", "kill");
    verifyTokeniserSuccess(&tokenise, "019kill", selector::T_NUMERIC_EXACT, "01", "9kill");
    verifyTokeniserSuccess(&tokenise, "0kill", selector::T_NUMERIC_EXACT, "0", "kill");
    verifyTokeniserSuccess(&tokenise, "0.kill", selector::T_NUMERIC_APPROX, "0.", "kill");
    verifyTokeniserSuccess(&tokenise, "3.1415=pi", selector::T_NUMERIC_APPROX, "3.1415", "=pi");
    verifyTokeniserSuccess(&tokenise, ".25.kill", selector::T_NUMERIC_APPROX, ".25", ".kill");
    verifyTokeniserSuccess(&tokenise, "2e5.kill", selector::T_NUMERIC_APPROX, "2e5", ".kill");
    verifyTokeniserSuccess(&tokenise, "3.e50easy to kill", selector::T_NUMERIC_APPROX, "3.e50", "easy to kill");
    verifyTokeniserSuccess(&tokenise, "34.25e+50easy to kill", selector::T_NUMERIC_APPROX, "34.25e+50", "easy to kill");
    verifyTokeniserSuccess(&tokenise, "34de", selector::T_NUMERIC_APPROX, "34d", "e");
    verifyTokeniserSuccess(&tokenise, "34fuller", selector::T_NUMERIC_APPROX, "34f", "uller");
    verifyTokeniserSuccess(&tokenise, "34Longer", selector::T_NUMERIC_EXACT, "34L", "onger");
    verifyTokeniserSuccess(&tokenise, "34littler", selector::T_NUMERIC_EXACT, "34l", "ittler");
    verifyTokeniserSuccess(&tokenise, "034Longer", selector::T_NUMERIC_EXACT, "034L", "onger");
    verifyTokeniserSuccess(&tokenise, "034littler", selector::T_NUMERIC_EXACT, "034l", "ittler");
    verifyTokeniserSuccess(&tokenise, "0X34littler", selector::T_NUMERIC_EXACT, "0X34l", "ittler");
    verifyTokeniserSuccess(&tokenise, "0X3456_fffflittler", selector::T_NUMERIC_EXACT, "0X3456_ffffl", "ittler");
    verifyTokeniserSuccess(&tokenise, "0xdead_beafittler", selector::T_NUMERIC_EXACT, "0xdead_beaf", "ittler");
}

SECTION("tokeniseFailure")
{
    verifyTokeniserFail(&tokeniseEos, "hb23");
    verifyTokeniserFail(&tokeniseIdentifier, "123");
    verifyTokeniserFail(&tokeniseIdentifier, "'Embedded 123'");
    verifyTokeniserFail(&tokeniseReservedWord, "1.2e5");
    verifyTokeniserFail(&tokeniseReservedWord, "'Stringy thing'");
    verifyTokeniserFail(&tokeniseReservedWord, "oR_andsomething");
    verifyTokeniserFail(&tokeniseString, "'Embedded 123");
    verifyTokeniserFail(&tokeniseString, "'This isn''t fair");
    verifyTokeniserFail(&tokeniseOperator, "123");
    verifyTokeniserFail(&tokeniseOperator, "'Stringy thing'");
    verifyTokeniserFail(&tokeniseOperator, "NoT");
    verifyTokeniserFail(&tokeniseOperator, "(a and b)");
    verifyTokeniserFail(&tokeniseOperator, ")");
    verifyTokeniserFail(&tokeniseParens, "=");
    verifyTokeniserFail(&tokeniseParens, "what ho!");
    verifyTokeniserFail(&tokeniseNumeric, "kill");
    verifyTokeniserFail(&tokeniseNumeric, "e3");
    verifyTokeniserFail(&tokeniseNumeric, "1.e.5");
    verifyTokeniserFail(&tokeniseNumeric, ".e5");
    verifyTokeniserFail(&tokeniseNumeric, "34e");
    verifyTokeniserFail(&tokeniseNumeric, ".3e+");
    verifyTokeniserFail(&tokeniseNumeric, ".3e-.");
    verifyTokeniserFail(&tokenise, "0b34Longer");
    verifyTokeniserFail(&tokenise, "0X_34Longer");
}

SECTION("tokenString")
{

    string exp("  a =b");
    Tokeniser t(exp);

    CHECK(t.nextToken() == Token(selector::T_IDENTIFIER, "a"));
    CHECK(t.nextToken() == Token(selector::T_EQUAL, "="));
    CHECK(t.nextToken() == Token(selector::T_IDENTIFIER, "b"));
    CHECK(t.nextToken() == Token(selector::T_EOS, ""));

    exp = " not 'hello kitty''s friend' = Is null       ";

    Tokeniser u(exp);

    CHECK(u.nextToken() == Token(selector::T_NOT, "not"));
    CHECK(u.nextToken() == Token(selector::T_STRING, "hello kitty's friend"));
    CHECK(u.nextToken() == Token(selector::T_EQUAL, "="));
    CHECK(u.nextToken() == Token(selector::T_IS, "Is"));
    CHECK(u.nextToken() == Token(selector::T_NULL, "null"));
    CHECK(u.nextToken() == Token(selector::T_EOS, ""));
    CHECK(u.nextToken() == Token(selector::T_EOS, ""));

    u.returnTokens(3);
    CHECK(u.nextToken() == Token(selector::T_IS, "Is"));
    CHECK(u.nextToken() == Token(selector::T_NULL, "null"));
    CHECK(u.nextToken() == Token(selector::T_EOS, ""));
    CHECK(u.nextToken() == Token(selector::T_EOS, ""));

    exp = "(a+6)*7.5/1e6";
    Tokeniser v(exp);

    CHECK(v.nextToken() == Token(selector::T_LPAREN, "("));
    CHECK(v.nextToken() == Token(selector::T_IDENTIFIER, "a"));
    CHECK(v.nextToken() == Token(selector::T_PLUS, "+"));
    CHECK(v.nextToken() == Token(selector::T_NUMERIC_EXACT, "6"));
    CHECK(v.nextToken() == Token(selector::T_RPAREN, ")"));
    CHECK(v.nextToken() == Token(selector::T_MULT, "*"));
    CHECK(v.nextToken() == Token(selector::T_NUMERIC_APPROX, "7.5"));
    CHECK(v.nextToken() == Token(selector::T_DIV, "/"));
    CHECK(v.nextToken() == Token(selector::T_NUMERIC_APPROX, "1e6"));
}

auto test_selector = [](const string& s) -> unique_ptr<Expression>
{
  INFO("String: " << s << " -> ");
  try {
    auto e = make_selector(s);
    if (e) {
        INFO("  Parse: " << *e);
    } else {
        INFO("  Null");
    }
    return e;
  } catch (std::exception& e) {
      INFO("  Exception: " << e.what());
      throw;
  }
};

SECTION("parseStringFail")
{
    CHECK_THROWS_AS(test_selector("hello world"), std::range_error);
    CHECK_THROWS_AS(test_selector("hello ^ world"), std::range_error);
    CHECK_THROWS_AS(test_selector("A is null not"), std::range_error);
    CHECK_THROWS_AS(test_selector("A is null or not"), std::range_error);
    CHECK_THROWS_AS(test_selector("A is null or and"), std::range_error);
    CHECK_THROWS_AS(test_selector("A is null and (B='hello out there'"), std::range_error);
    CHECK_THROWS_AS(test_selector("in='hello kitty'"), std::range_error);
    CHECK_THROWS_AS(test_selector("A like 234"), std::range_error);
    CHECK_THROWS_AS(test_selector("A not 234 escape"), std::range_error);
    CHECK_THROWS_AS(test_selector("A not like 'eclecti_' escape 'happy'"), std::range_error);
    CHECK_THROWS_AS(test_selector("A not like 'eclecti_' escape happy"), std::range_error);
    CHECK_THROWS_AS(test_selector("A not like 'eclecti_' escape '%'"), std::range_error);
    CHECK_THROWS_AS(test_selector("A BETWEEN AND 'true'"), std::range_error);
    CHECK_THROWS_AS(test_selector("A NOT BETWEEN 34 OR 3.9"), std::range_error);
    CHECK_THROWS_AS(test_selector("A IN ()"), std::range_error);
    CHECK_THROWS_AS(test_selector("A NOT IN ()"), std::range_error);
    CHECK_THROWS_AS(test_selector("A IN 'hello', 'there', 1, true, (1-17))"), std::range_error);
    CHECK_THROWS_AS(test_selector("A IN ('hello', 'there' 1, true, (1-17))"), std::range_error);
}

SECTION("parseString")
{
    CHECK_NOTHROW(test_selector("'Daft' is not null"));
    CHECK_NOTHROW(test_selector("42 is null"));
    CHECK_NOTHROW(test_selector("A is not null"));
    CHECK_NOTHROW(test_selector("A is null"));
    CHECK_NOTHROW(test_selector("A = C"));
    CHECK_NOTHROW(test_selector("A <> C"));
    CHECK_NOTHROW(test_selector("A='hello kitty'"));
    CHECK_NOTHROW(test_selector("A<>'hello kitty'"));
    CHECK_NOTHROW(test_selector("A=B"));
    CHECK_NOTHROW(test_selector("A<>B"));
    CHECK_NOTHROW(test_selector("A='hello kitty' OR B='Bye, bye cruel world'"));
    CHECK_NOTHROW(test_selector("B='hello kitty' AnD A='Bye, bye cruel world'"));
    CHECK_NOTHROW(test_selector("A is null or A='Bye, bye cruel world'"));
    CHECK_NOTHROW(test_selector("Z is null OR A is not null and A<>'Bye, bye cruel world'"));
    CHECK_NOTHROW(test_selector("(Z is null OR A is not null) and A<>'Bye, bye cruel world'"));
    CHECK_NOTHROW(test_selector("NOT C is not null OR C is null"));
    CHECK_NOTHROW(test_selector("Not A='' or B=z"));
    CHECK_NOTHROW(test_selector("Not A=17 or B=5.6"));
    CHECK_NOTHROW(test_selector("A<>17 and B=5.6e17"));
    CHECK_NOTHROW(test_selector("A LIKE 'excep%ional'"));
    CHECK_NOTHROW(test_selector("B NOT LIKE 'excep%ional'"));
    CHECK_NOTHROW(test_selector("A LIKE 'excep%ional' EScape '\'"));
    CHECK_NOTHROW(test_selector("A BETWEEN 13 AND 'true'"));
    CHECK_NOTHROW(test_selector("A NOT BETWEEN 100 AND 3.9"));
    CHECK_NOTHROW(test_selector("true"));
    CHECK_NOTHROW(test_selector("-354"));
    CHECK_NOTHROW(test_selector("-(X or Y)"));
    CHECK_NOTHROW(test_selector("-687 or 567"));
    CHECK_NOTHROW(test_selector("(354.6)"));
    CHECK_NOTHROW(test_selector("A is null and 'hello out there'"));
    CHECK_NOTHROW(test_selector("17/4>4"));
    CHECK_NOTHROW(test_selector("17/4>+4"));
    CHECK_NOTHROW(test_selector("17/4>-4"));
    CHECK_NOTHROW(test_selector("A IN ('hello', 'there', 1 , true, (1-17))"));
}

static constexpr selector::Value EMPTY{};

class TestSelectorEnv : public Env {
    mutable unordered_map<string, selector::Value> values;
    vector<string> strings;

    const selector::Value& value(string_view v) const override {
        auto s = string{v};
        const selector::Value& r = values.find(s)!=values.end() ? values[s] : EMPTY;
        INFO("  Lookup: " << v << " -> " << r);
        return r;
    }

public:
    void set(const string& id, string_view value) {
        strings.push_back(string{value});
        values[id] = string_view{strings[strings.size()-1]};
    }

    void set(const string& id, const selector::Value& value) {
        if (value.type==selector::Value::T_STRING) {
            set(id, get<string_view>(value.value));
        } else {
            values[id] = value;
        }
    }
};

auto eval_selector = [&test_selector](const string& s, const TestSelectorEnv& e) -> bool
{
    auto exp = test_selector(s);
    return eval(*exp, e);
};

SECTION("simpleEval")
{
    TestSelectorEnv env;
    env.set("A", "Bye, bye cruel world"sv);
    env.set("B", "hello kitty"sv);

    CHECK(eval_selector("", env));
    CHECK(eval_selector(" ", env));
    CHECK(eval_selector("A is not null", env));
    CHECK(!eval_selector("A is null", env));
    CHECK(!eval_selector("A = C", env));
    CHECK(!eval_selector("A <> C", env));
    CHECK(!eval_selector("C is not null", env));
    CHECK(eval_selector("C is null", env));
    CHECK(eval_selector("A='Bye, bye cruel world'", env));
    CHECK(!eval_selector("A<>'Bye, bye cruel world'", env));
    CHECK(!eval_selector("A='hello kitty'", env));
    CHECK(eval_selector("A<>'hello kitty'", env));
    CHECK(!eval_selector("A=B", env));
    CHECK(eval_selector("A<>B", env));
    CHECK(!eval_selector("A='hello kitty' OR B='Bye, bye cruel world'", env));
    CHECK(eval_selector("B='hello kitty' OR A='Bye, bye cruel world'", env));
    CHECK(eval_selector("B='hello kitty' AnD A='Bye, bye cruel world'", env));
    CHECK(!eval_selector("B='hello kitty' AnD B='Bye, bye cruel world'", env));
    CHECK(eval_selector("A is null or A='Bye, bye cruel world'", env));
    CHECK(eval_selector("Z is null OR A is not null and A<>'Bye, bye cruel world'", env));
    CHECK(!eval_selector("(Z is null OR A is not null) and A<>'Bye, bye cruel world'", env));
    CHECK(eval_selector("NOT C is not null OR C is null", env));
    CHECK(eval_selector("Not A='' or B=z", env));
    CHECK(eval_selector("Not A=17 or B=5.6", env));
    CHECK(!eval_selector("A<>17 and B=5.6e17", env));
    CHECK(!eval_selector("C=D", env));
    CHECK(eval_selector("13 is not null", env));
    CHECK(!eval_selector("'boo!' is null", env));
    CHECK(eval_selector("A LIKE '%cru_l%'", env));
    CHECK(eval_selector("'_%%_hello.th_re%' LIKE 'z_%.%z_%z%' escape 'z'", env));
    CHECK(eval_selector("A NOT LIKE 'z_%.%z_%z%' escape 'z'", env));
    CHECK(eval_selector("'{}[]<>,.!\"$%^&*()_-+=?/|\\' LIKE '{}[]<>,.!\"$z%^&*()z_-+=?/|\\' escape 'z'", env));
}

SECTION("numericEval")
{
    TestSelectorEnv env;
    env.set("A", 42.0);
    env.set("B", 39);

    CHECK(eval_selector("A>B", env));
    CHECK(eval_selector("A=42", env));
    CHECK(eval_selector("42=A", env));
    CHECK(eval_selector("B=39.0", env));
    CHECK(eval_selector("Not A=17 or B=5.6", env));
    CHECK(!eval_selector("A<>17 and B=5.6e17", env));
    CHECK(eval_selector("3 BETWEEN -17 and 98.5", env));
    CHECK(eval_selector("A BETWEEN B and 98.5", env));
    CHECK(!eval_selector("B NOT BETWEEN 35 AND 100", env));
    CHECK(!eval_selector("A BETWEEN B and 40", env));
    CHECK(!eval_selector("A BETWEEN C and 40", env));
    CHECK(!eval_selector("A BETWEEN 45 and C", env));
    CHECK(eval_selector("(A BETWEEN 40 and C) IS NULL", env));
    CHECK(eval_selector("(A BETWEEN C and 45) IS NULL", env));
    CHECK(eval_selector("17/4=4", env));
    CHECK(!eval_selector("A/0=0", env));
    CHECK(eval_selector("A*B+19<A*(B+19)", env));
    CHECK(eval_selector("-A=0-A", env));
}

SECTION("numericLiterals")
{
    TestSelectorEnv env;

    CHECK(eval_selector(" 9223372036854775807>0", env));
    CHECK(eval_selector("-9223372036854775807<0", env));
    CHECK_THROWS_AS(eval_selector(" 9223372036854775808>0", env), std::range_error);
    CHECK(eval_selector("0x8000_0000_0000_0001=-9223372036854775807", env));
    CHECK_THROWS_AS(eval_selector("-9223372036854775809<0", env), std::range_error);
    CHECK(eval_selector(" 9223372036854775807L<>0", env));
    CHECK(eval_selector("-9223372036854775807L<>0", env));
    CHECK(eval_selector("-9223372036854775808<>0", env));
    CHECK(eval_selector("-9223372036854775808=0x8000_0000_0000_0000", env));
    CHECK(eval_selector("0x8000_0000_0000_0000<9223372036854775807", env));
    CHECK(eval_selector(" 0.4f>0.3d", env));
    CHECK(eval_selector(" 1000_020.4f>0.3d", env));
    CHECK(eval_selector(" 1000_020.4f>0x800p-3", env));
    CHECK(eval_selector(" 0x1000_0000=0x1000_0000p0", env));
    CHECK(eval_selector(" 0xFF=255L", env));
    CHECK(eval_selector(" 077L=0b111_111", env));
    CHECK(eval_selector(" 077L=63", env));
}

SECTION("comparisonEval")
{
    TestSelectorEnv env;

    CHECK(!eval_selector("17 > 19.0", env));
    CHECK(!eval_selector("'hello' > 19.0", env));
    CHECK(!eval_selector("'hello' < 19.0", env));
    CHECK(!eval_selector("'hello' = 19.0", env));
    CHECK(!eval_selector("'hello'>42 and 'hello'<42 and 'hello'=42 and 'hello'<>42", env));
    CHECK(eval_selector("20 >= 19.0 and 20 > 19", env));
    CHECK(eval_selector("42 <= 42.0 and 37.0 >= 37", env));
    CHECK(eval_selector("(A IN ('hello', 'there', 1 , true, (1-17))) IS NULL", env));
    CHECK(eval_selector("(-16 IN ('hello', A, 'there', true)) IS NULL", env));
    CHECK(eval_selector("(-16 NOT IN ('hello', 'there', A, true)) IS NULL", env));
    CHECK(eval_selector("(-16 IN ('hello', 'there', true)) IS NOT NULL", env));
    CHECK(!eval_selector("-16 IN ('hello', 'there', true)", env));
    CHECK(eval_selector("(-16 NOT IN ('hello', 'there', true)) IS NOT NULL", env));
    CHECK(!eval_selector("-16 NOT IN ('hello', 'there', true)", env));
    CHECK(eval_selector("(-16 NOT IN ('hello', 'there', A, 1 , true)) IS NULL", env));
    CHECK(eval_selector("'hello' IN ('hello', 'there', 1 , true, (1-17))", env));
    CHECK(eval_selector("TRUE IN ('hello', 'there', 1 , true, (1-17))", env));
    CHECK(eval_selector("-16 IN ('hello', 'there', 1 , true, (1-17))", env));
    CHECK(!eval_selector("-16 NOT IN ('hello', 'there', 1 , true, (1-17))", env));
    CHECK(!eval_selector("1 IN ('hello', 'there', 'polly')", env));
    CHECK(!eval_selector("1 NOT IN ('hello', 'there', 'polly')", env));
    CHECK(!eval_selector("'hell' IN ('hello', 'there', 1 , true, (1-17))", env));
    CHECK(eval_selector("('hell' IN ('hello', 'there', 1 , true, (1-17), A)) IS NULL", env));
    CHECK(eval_selector("('hell' NOT IN ('hello', 'there', 1 , true, (1-17), A)) IS NULL", env));
    CHECK(!eval_selector("'hello kitty' BETWEEN 30 and 40", env));
    CHECK(eval_selector("'hello kitty' NOT BETWEEN 30 and 40", env));
    CHECK(!eval_selector("14 BETWEEN 'aardvark' and 'zebra'", env));
    CHECK(eval_selector("14 NOT BETWEEN 'aardvark' and 'zebra'", env));
    CHECK(!eval_selector("TRUE BETWEEN 'aardvark' and 'zebra'", env));
    CHECK(eval_selector("TRUE NOT BETWEEN 'aardvark' and 'zebra'", env));
    CHECK(eval_selector("(A BETWEEN 'aardvark' and 14) IS NULL", env));
    CHECK(eval_selector("(A NOT BETWEEN 'aardvark' and 14) IS NULL", env));
    CHECK(eval_selector("(14 BETWEEN A and 17) IS NULL", env));
    CHECK(eval_selector("(14 NOT BETWEEN A and 17) IS NULL", env));
    CHECK(eval_selector("(14 BETWEEN 11 and A) IS NULL", env));
    CHECK(eval_selector("(14 NOT BETWEEN 11 and A) IS NULL", env));
    CHECK(eval_selector("14 NOT BETWEEN 11 and 9", env));
    CHECK(eval_selector("14 BETWEEN -11 and 54367", env));
}

SECTION("NullEval")
{
    TestSelectorEnv env;

    CHECK(eval_selector("P > 19.0 or (P is null)", env));
    CHECK(eval_selector("P is null or P=''", env));
    CHECK(!eval_selector("P=Q", env));
    CHECK(!eval_selector("not P=Q", env));
    CHECK(!eval_selector("not P=Q and not P=Q", env));
    CHECK(!eval_selector("P=Q or not P=Q", env));
    CHECK(!eval_selector("P > 19.0 or P <= 19.0", env));
    CHECK(eval_selector("P > 19.0 or 17 <= 19.0", env));
}

}

}
