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
#include <ostream>

using std::ostream;

namespace selector {

ostream& operator<<(ostream& os, const Value& v)
{
    switch (v.type) {
    case Value::T_UNKNOWN: os << "UNKNOWN"; break;
    case Value::T_BOOL: os << "BOOL:" << std::boolalpha << v.b; break;
    case Value::T_EXACT: os << "EXACT:" << v.i; break;
    case Value::T_INEXACT: os << "APPROX:" << v.x; break;
    case Value::T_STRING: os << "STRING:'" << *v.s << "'"; break;
    };
    return os;
}

void promoteNumeric(Value& v1, Value& v2)
{
    if (!numeric(v1) || !numeric(v2)) return;
    if (sameType(v1,v2)) return;
    switch (v1.type) {
    case Value::T_INEXACT: v2 = double(v2.i); return;
    case Value::T_EXACT:   v1 = double(v1.i); return;
    default:               assert(false);
    }
}

bool operator==(Value v1, Value v2)
{
    promoteNumeric(v1, v2);
    if (!sameType(v1,v2)) return false;

    switch (v1.type) {
    case Value::T_BOOL:    return  v1.b == v2.b;
    case Value::T_STRING:  return *v1.s == *v2.s;
    case Value::T_EXACT:   return  v1.i == v2.i;
    case Value::T_INEXACT: return  v1.x == v2.x;
    case Value::T_UNKNOWN: return false;
    }
}

bool operator!=(Value v1, Value v2)
{
    promoteNumeric(v1, v2);
    if (!sameType(v1,v2)) return false;

    switch (v1.type) {
    case Value::T_BOOL:    return  v1.b != v2.b;
    case Value::T_STRING:  return *v1.s != *v2.s;
    case Value::T_EXACT:   return  v1.i != v2.i;
    case Value::T_INEXACT: return  v1.x != v2.x;
    case Value::T_UNKNOWN: return false;
    }
}

bool operator<(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i < v2.i;
    case Value::T_INEXACT: return v1.x < v2.x;
    default:               break;
    }
    return false;
}

bool operator>(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i > v2.i;
    case Value::T_INEXACT: return v1.x > v2.x;
    default:               break;
    }
    return false;
}

bool operator<=(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i <= v2.i;
    case Value::T_INEXACT: return v1.x <= v2.x;
    default:               break;
    }
    return false;
}

bool operator>=(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i >= v2.i;
    case Value::T_INEXACT: return v1.x >= v2.x;
    default:               break;
    }
    return false;
}

BoolOrNone operator!(const Value& v)
{
    switch (v.type) {
    case Value::T_BOOL:    return BoolOrNone(!v.b);
    default:               break;
    }
    return BN_UNKNOWN;
}

Value operator+(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i + v2.i;
    case Value::T_INEXACT: return v1.x + v2.x;
    default:               break;
    }
    return Value();
}

Value operator-(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i - v2.i;
    case Value::T_INEXACT: return v1.x - v2.x;
    default:               break;
    }
    return Value();
}

Value operator*(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i * v2.i;
    case Value::T_INEXACT: return v1.x * v2.x;
    default:               break;
    }
    return Value();
}

Value operator/(Value v1, Value v2)
{
    promoteNumeric(v1, v2);

    switch (v1.type) {
    case Value::T_EXACT:   return v1.i / v2.i;
    case Value::T_INEXACT: return v1.x / v2.x;
    default:               break;
    }
    return Value();
}

Value operator-(const Value& v)
{
    switch (v.type) {
    case Value::T_EXACT:   return -v.i;
    case Value::T_INEXACT: return -v.x;
    default:               break;
    }
    return Value();
}

}
