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
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;

// C interfaces

struct selector_expression_t : selector::Expression {};

struct selector_value_t : selector::Value {};

auto constexpr EMPTY = selector::Value{};

struct selector_environment_t : selector::Env {
    unordered_map<string_view, unique_ptr<const selector::Value>> values;

    auto value(const string_view sv) const -> const selector::Value& override final {
        if (auto i = values.find(string{sv}); i!=values.end()) {
                return *i->second;
        }
        return EMPTY;
    }

    auto set(string_view var, unique_ptr<const selector::Value> val) -> void {
	    values[var] = std::move(val);
    }
};

auto selector_intern(string_view str) -> const char* {
    static auto strings = unordered_set<string>{};

    string s{str};
    if (auto i = strings.find(s); i!=strings.end()) return i->c_str();
    else return strings.emplace(s).first->c_str();
}

auto selector_intern(const char* str) -> const char* {
    return selector_intern(string_view{str});
}

auto selector_expression(const char* exp) -> const selector_expression_t* {
    try {
        return static_cast<selector_expression_t*>(selector::make_selector(exp).release());
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return nullptr;
    }
}

auto selector_expression_free(const selector_expression_t* exp) -> void {
    delete exp;
}

auto selector_expression_eval(const selector_expression_t* exp, const selector_environment_t* env) -> bool {
    return eval(*exp, *env);
}

auto selector_expression_value(const selector_expression_t* exp, const selector_environment_t* env) -> const selector_value_t* {
    auto val = exp->eval(*env);
    if (selector::characters(val)) val.value = selector_intern(std::get<string_view>(val.value));
    return static_cast<selector_value_t*>(new selector::Value{val});
}

auto selector_expression_dump(const selector_expression_t* exp) -> void{
    std::cerr << *exp;
}

auto selector_environment() -> selector_environment_t* {
    return new selector_environment_t;
}

auto selector_environment_free(const selector_environment_t* env) -> void {
    delete env;
}

auto selector_environment_dump(const selector_environment_t* env) -> void {
    for (auto& [k, v] : env->values) {
        std::cerr << k << "=" << *v << "\n";
    }
}

auto selector_environment_set(selector_environment_t* env, const char* var, const selector_value_t* val) -> void {
    env->set(var, unique_ptr<const selector::Value>{val});
}

auto selector_environment_get(selector_environment_t* env, const char* var) -> const selector_value_t* {
    return static_cast<const selector_value_t*>(&env->value(var));
}

auto selector_value_unknown() -> const selector_value_t* {
    return static_cast<const selector_value_t*>(&EMPTY);
}

auto selector_value_bool(bool b) -> const selector_value_t* {
    return static_cast<const selector_value_t*>(new selector::Value(b));
}

auto selector_value_exact(int64_t i) -> const selector_value_t* {
    return static_cast<const selector_value_t*>(new selector::Value(i));
}

auto selector_value_approx(double d) -> const selector_value_t* {
    return static_cast<const selector_value_t*>(new selector::Value(d));
}

auto selector_value_string(const char* str) -> const selector_value_t* {
    return static_cast<const selector_value_t*>(new selector::Value(selector_intern(str)));
}

auto selector_value(const char* str) -> const selector_value_t* {
    auto selector_env = std::unique_ptr<const selector_environment_t>{selector_environment()};
    auto selector_exp = std::unique_ptr<const selector_expression_t>{selector_expression(str)};
    return selector_expression_value(selector_exp.get(), selector_env.get());
}

auto selector_value_free(const selector_value_t* v) -> void {
    if (v != selector_value_unknown()) delete v;
}

auto selector_value_dump(const selector_value_t* v) -> void {
    std::cerr << *v;
}

