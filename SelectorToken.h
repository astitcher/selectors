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

#include "selectors_export.h"

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

    Token()
    {}

    Token(TokenType t, std::string_view v) :
        type(t),
        val(v)
    {}

    bool operator==(const Token& r) const
    {
        return
            (type == T_EOS && r.type == T_EOS) ||
            (type == r.type && val == r.val);
    }
};

SELECTORS_EXPORT
std::ostream& operator<<(std::ostream& os, const Token& t);

class SELECTORS_EXPORT
TokenException : public std::range_error {
public:
    TokenException(const std::string&);
};


SELECTORS_EXPORT
bool tokenise(std::string_view& sv, Token& tok);

class
Tokeniser {
    std::vector<Token> tokens;
    unsigned int tokp;

    std::string_view input;
    std::string_view::const_iterator inp;

public:
    SELECTORS_EXPORT Tokeniser(std::string_view input);
    SELECTORS_EXPORT void returnTokens(unsigned int n = 1);
    SELECTORS_EXPORT const Token& nextToken();
    SELECTORS_EXPORT std::string_view remaining();
};

}

#endif
