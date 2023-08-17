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

#include "SelectorValue.h"

#include <cassert>
#include <cstdint>
#include <ostream>
#include <variant>

using std::boolalpha;
using std::get;
using std::ostream;
using std::string_view;

// Define overload until C++26!
template <class ...Fs>
struct overload : Fs... {
  template <class ...Ts>
  overload(Ts&& ...ts) : Fs{std::forward<Ts>(ts)}...
  {}

  using Fs::operator()...;
};

template <class ...Ts>
overload(Ts&&...) -> overload<std::remove_reference_t<Ts>...>;

namespace selector {

ostream& operator<<(ostream& os, const Value& v)
{
    std::visit(overload(
        [&](std::monostate) { os << "UNKNOWN"; },
        [&](bool b) {os << "BOOL:" << boolalpha << b; },
        [&](int64_t i) { os << "EXACT:" << i; },
        [&](double d) { os << "APPROX:" << d; },
        [&](string_view sv) { os << "STRING:'" << sv << "'"; }
    ), v.value);
    return os;
}

inline bool promoteNumeric(Value& v1, Value& v2)
{
    if (!numeric(v1) || !numeric(v2)) return false;
    if (sameType(v1,v2)) return true;
    switch (v1.type()) {
    case Value::T_INEXACT: v2 = double(get<int64_t>(v2.value)); return true;
    case Value::T_EXACT:   v1 = double(get<int64_t>(v1.value)); return true;
    default:               assert(false);
    }
}

bool operator==(Value v1, Value v2)
{
    promoteNumeric(v1, v2);
    if (!sameType(v1,v2)) return false;

    return v1.value == v2.value;
}

bool operator!=(Value v1, Value v2)
{
    promoteNumeric(v1, v2);
    if (!sameType(v1,v2)) return false;

    return v1.value != v2.value;
}

bool operator<(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return false;

    switch (v1.type()) {
    case Value::T_EXACT:
    case Value::T_INEXACT:
        return v1.value < v2.value;
    default:
        assert(false);
    }
}

bool operator>(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return false;

    switch (v1.type()) {
    case Value::T_EXACT:
    case Value::T_INEXACT:
        return v1.value > v2.value;
    default:
        assert(false);
    }
}

bool operator<=(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return false;

    switch (v1.type()) {
    case Value::T_EXACT:
    case Value::T_INEXACT:
        return v1.value <= v2.value;
    default:
        assert(false);
    }
}

bool operator>=(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return false;

    switch (v1.type()) {
    case Value::T_EXACT:
    case Value::T_INEXACT:
        return v1.value >= v2.value;
    default:
        assert(false);
    }
}

BoolOrNone operator!(const Value& v)
{
    switch (v.type()) {
    case Value::T_BOOL:
        return BoolOrNone(!get<bool>(v.value));
    default:
        break;
    }
    return BN_UNKNOWN;
}

Value operator+(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return Value{};

    switch (v1.type()) {
    case Value::T_EXACT:
        return get<int64_t>(v1.value) + get<int64_t>(v2.value);
    case Value::T_INEXACT:
        return get<double>(v1.value) + get<double>(v2.value);
    default:
        assert(false);
    }
}

Value operator-(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return Value{};

    switch (v1.type()) {
    case Value::T_EXACT:
        return get<int64_t>(v1.value) - get<int64_t>(v2.value);
    case Value::T_INEXACT:
        return get<double>(v1.value) - get<double>(v2.value);
    default:
        assert(false);
    }
}

Value operator*(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return Value{};

    switch (v1.type()) {
    case Value::T_EXACT:
        return get<int64_t>(v1.value) * get<int64_t>(v2.value);
    case Value::T_INEXACT:
        return get<double>(v1.value) * get<double>(v2.value);
    default:
        assert(false);
    }
}

Value operator/(Value v1, Value v2)
{
    if (!promoteNumeric(v1, v2)) return Value{};

    switch (v1.type()) {
    case Value::T_EXACT:
        return get<int64_t>(v1.value) / get<int64_t>(v2.value);
    case Value::T_INEXACT:
        return get<double>(v1.value) / get<double>(v2.value);
    default:
        assert(false);
    }
}

Value operator-(const Value& v)
{
    switch (v.type()) {
    case Value::T_EXACT:
        return -get<int64_t>(v.value);
    case Value::T_INEXACT:
        return -get<double>(v.value);
    default:
        break;
    }
    return Value();
}

}
