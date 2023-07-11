#ifndef SELECTOR_VALUE_H
#define SELECTOR_VALUE_H

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

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <variant>

#include "selectors_export.h"

namespace selector {

enum BoolOrNone : uint8_t {
    BN_FALSE = false,
    BN_TRUE = true,
    BN_UNKNOWN
};

// The user of the Value class for strings must ensure that
// the string has a lifetime longer than the string used and
// is responsible for managing its lifetime.
class Value {
public:
    std::variant<std::monostate, bool, int64_t, double, std::string_view> value;
    // NB: Must keep this in the same order as the variant or strange things will happen
    enum : uint8_t {
        T_UNKNOWN,
        T_BOOL,
        T_EXACT,
        T_INEXACT,
        T_STRING
    };

    Value(const Value&) = default;
    Value& operator=(const Value&) = default;
    ~Value() noexcept = default;

    constexpr size_t type() const {
        return value.index();
    }

    constexpr Value()
    {}

    constexpr Value(std::string_view s0) :
        value(s0)
    {}

    constexpr Value(const int64_t i0) :
        value(i0)
    {}

    constexpr Value(const int32_t i0) :
        value(i0)
    {}

    constexpr Value(const double x0) :
        value(x0)
    {}

    constexpr Value(bool b0) :
        value(b0)
    {}

    constexpr Value(BoolOrNone bn) :
        value((bool)bn)
    { if (bn==BN_UNKNOWN) value=std::monostate{}; }

    constexpr operator BoolOrNone() const {
        if (value.index() == T_BOOL) return BoolOrNone{std::get<bool>(value)};
        else return BN_UNKNOWN;
    }
};

inline constexpr bool unknown(const Value& v) {
    return v.value.index() == Value::T_UNKNOWN;
}

inline constexpr bool numeric(const Value& v) {
    return v.value.index() == Value::T_EXACT || v.value.index() == Value::T_INEXACT;
}

inline constexpr bool sameType(const Value& v1, const Value& v2) {
    return v1.value.index() == v2.value.index();
}


bool operator==(Value, Value);
bool operator!=(Value, Value);
bool operator<(Value, Value);
bool operator>(Value, Value);
bool operator<=(Value, Value);
bool operator>=(Value, Value);
BoolOrNone operator!(const Value&);

Value operator+(Value, Value);
Value operator-(Value, Value);
Value operator*(Value, Value);
Value operator/(Value, Value);
Value operator-(const Value&);

SELECTORS_EXPORT std::ostream& operator<<(std::ostream& os, const Value& v);

}

#endif
