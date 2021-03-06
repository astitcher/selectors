#ifndef SELECTOR_TOKEN_H
#define SELECTOR_TOKEN_H

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

#include <iosfwd>
#include <string>
#include <stdexcept>
#include <vector>

namespace selector {

typedef enum {
    T_EOS,
    T_NULL,
    T_TRUE,
    T_FALSE,
    T_NOT,
    T_AND,
    T_OR,
    T_IN,
    T_IS,
    T_BETWEEN,
    T_LIKE,
    T_ESCAPE,
    T_IDENTIFIER,
    T_STRING,
    T_NUMERIC_EXACT,
    T_NUMERIC_APPROX,
    T_LPAREN,
    T_RPAREN,
    T_COMMA,
    T_PLUS,
    T_MINUS,
    T_MULT,
    T_DIV,
    T_EQUAL,
    T_NEQ,
    T_LESS,
    T_GRT,
    T_LSEQ,
    T_GREQ
} TokenType;

struct Token {
    TokenType type;
    std::string val;
    std::string::const_iterator tokenStart;

    Token()
    {}

    Token(TokenType t, const std::string& v) :
        type(t),
        val(v)
    {}

    Token(TokenType t, const std::string::const_iterator& s, const std::string& v) :
        type(t),
        val(v),
        tokenStart(s)
    {}

    Token(TokenType t, const std::string::const_iterator& s, const std::string::const_iterator& e) :
        type(t),
        val(std::string(s,e)),
        tokenStart(s)
    {}

    bool operator==(const Token& r) const
    {
        return
            (type == T_EOS && r.type == T_EOS) ||
            (type == r.type && val == r.val);
    }
};

__attribute__((visibility("default")))
std::ostream& operator<<(std::ostream& os, const Token& t);

class __attribute__((visibility("default")))
TokenException : public std::range_error {
public:
    TokenException(const std::string&);
};


__attribute__((visibility("default")))
bool tokenise(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

class
Tokeniser {
    std::vector<Token> tokens;
    unsigned int tokp;

    std::string::const_iterator inStart;
    std::string::const_iterator inp;
    std::string::const_iterator inEnd;

public:
    __attribute__((visibility("default"))) Tokeniser(const std::string::const_iterator& s, const std::string::const_iterator& e);
    __attribute__((visibility("default"))) void returnTokens(unsigned int n = 1);
    __attribute__((visibility("default"))) const Token& nextToken();
    __attribute__((visibility("default"))) std::string remaining();
};

}

#endif
