#ifndef SELECTOR_OPERATORS_H
#define SELECTOR_OPERATORS_H

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

#include "SelectorInternal.h"
#include "SelectorValue.h"

#include <string>
#include <vector>
#include <ostream>

namespace selector {

class ValueExpression;  // Forward declaration

class Operator {
    const char* repr_;

public:
    constexpr Operator(const char* r);
    auto repr(std::ostream& o) const -> void;
};

// Function type definitions for operators
using CompFn = auto (Value, Value) -> bool;
using UBoolFn = auto (const Value&) -> BoolOrNone;
using ArithFn = auto (Value, Value) -> Value;
using UArithFn = auto (const Value&) -> Value;

// Comparison operators
class ComparisonOperator : public Operator {
    CompFn& fn_;

public:
    constexpr ComparisonOperator(const char* r, CompFn* fn);
    auto eval(ValueExpression& e1, ValueExpression& e2, 
             const std::vector<Value>& vals) const -> BoolOrNone;
};

// Unary boolean operators
class UnaryBooleanOperator : public Operator {
    UBoolFn& fn_;

public:
    constexpr UnaryBooleanOperator(const char* r, UBoolFn* fn);
    auto eval(ValueExpression& e, 
             const std::vector<Value>& vals) const -> BoolOrNone;
};

// Arithmetic operators
class ArithmeticOperator : public Operator {
    ArithFn& fn_;

public:
    constexpr ArithmeticOperator(const char* r, ArithFn* fn);
    auto eval(ValueExpression& e1, ValueExpression& e2,
             const std::vector<Value>& vals) const -> Value;
};

// Unary arithmetic operators
class UnaryArithmeticOperator : public Operator {
    UArithFn& fn_;

public:
    constexpr UnaryArithmeticOperator(const char* r, UArithFn* fn);
    auto eval(ValueExpression& e,
             const std::vector<Value>& vals) const -> Value;
};

// Template specialization declarations
template <> struct has_repr<ComparisonOperator> {static constexpr bool value = true;};
template <> struct has_repr<UnaryBooleanOperator> {static constexpr bool value = true;};
template <> struct has_repr<ArithmeticOperator> {static constexpr bool value = true;};
template <> struct has_repr<UnaryArithmeticOperator> {static constexpr bool value = true;};

constexpr Operator::Operator(const char* r) :
    repr_(r)
{}

constexpr ComparisonOperator::ComparisonOperator(const char* r, CompFn* fn) :
    Operator(r),
    fn_(*fn)
{}

constexpr UnaryBooleanOperator::UnaryBooleanOperator(const char* r, UBoolFn* fn) :
    Operator(r),
    fn_(*fn)
{}

constexpr ArithmeticOperator::ArithmeticOperator(const char* r, ArithFn* fn) :
    Operator(r),
    fn_(*fn)
{}

constexpr UnaryArithmeticOperator::UnaryArithmeticOperator(const char* r, UArithFn* fn) :
    Operator(r),
    fn_(*fn)
{}

// Operator instances
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

} // namespace selector

#endif  // SELECTOR_OPERATORS_H