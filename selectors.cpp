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

#include "selectors.h"

#include "SelectorExpression.h"
#include "SelectorEnv.h"
#include "SelectorToken.h"
#include "SelectorValue.h"

#include <iostream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

using std::string;
using std::string_view;
using std::unordered_map;
using std::unordered_set;

// C interfaces

struct selector_expression_t : selector::Expression {};

struct selector_value_t : selector::Value {};

auto constexpr EMPTY = selector::Value{};

struct selector_environment_t : selector::Env {
    unordered_map<string_view, selector::Value> values;

    const selector::Value & value(const string_view sv) const override {
	auto i = values.find(string{sv});
	if (i != values.end()) {
            auto [_, v] = *i;
            return v;
	}
	return EMPTY;
    }

    void set(string_view var, const selector::Value& val) {
	values[var] = val;
    }
};

const char* selector_intern(const char* str) {
    static auto strings = unordered_set<string>{};

    if (auto i=strings.find(str); i!=strings.end()) return i->c_str();
    else return strings.emplace(str).first->c_str();
}

const selector_expression_t* selector_expression(const char* exp) {
    return static_cast<selector_expression_t*>(selector::make_selector(exp).release());
}

void selector_expression_free(const selector_expression_t* exp) {
    delete exp;
}

bool selector_expression_eval(const selector_expression_t* exp, const selector_environment_t* env) {
    return eval(*exp, *env);
}

void selector_expression_dump(const selector_expression_t* exp) {
    std::cerr << *exp;
}

selector_environment_t* selector_environment() {
    return new selector_environment_t;
}

void selector_environment_free(const selector_environment_t* env) {
    delete env;
}

void selector_environment_set(selector_environment_t* env, const char* var, const selector_value_t* val) {
    env->set(var, *val);
}

const selector_value_t* selector_value_unknown() {
    return static_cast<const selector_value_t*>(&EMPTY);
}

const selector_value_t* selector_value_bool(bool b) {
    return static_cast<const selector_value_t*>(new selector::Value(b));
}

const selector_value_t* selector_value_exact(int64_t i) {
    return static_cast<const selector_value_t*>(new selector::Value(i));
}

const selector_value_t* selector_value_approx(double d) {
    return static_cast<const selector_value_t*>(new selector::Value(d));
}

const selector_value_t* selector_value_string(const char* str) {
    return static_cast<const selector_value_t*>(new selector::Value(str));
}

void selector_value_free(const selector_value_t* v) {
    if (v!=selector_value_unknown()) delete v;
}

void selector_value_dump(const selector_value_t* v) {
    std::cerr << *v;
}

