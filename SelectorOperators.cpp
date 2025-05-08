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

#include "SelectorOperators.h"

#include "SelectorExpression.h"
#include "SelectorInternal.h"
#include "SelectorValue.h"

#include <ostream>
#include <string>
#include <vector>

using std::ostream;
using std::string;
using std::vector;

namespace selector {

auto Operator::repr(ostream& o) const -> void {
    o << repr_;
}

auto ComparisonOperator::eval(ValueExpression& e1, ValueExpression& e2, const vector<Value>& vals) const -> BoolOrNone {
    const Value v1(e1.eval(vals));
    if (!unknown(v1)) {
        const Value v2(e2.eval(vals));
        if (!unknown(v2)) {
            return BoolOrNone(fn_(v1, v2));
        }
    }
    return BN_UNKNOWN;
}

auto UnaryBooleanOperator::eval(ValueExpression& e, const vector<Value>& vals) const -> BoolOrNone {
    return fn_(e.eval(vals));
}

auto ArithmeticOperator::eval(ValueExpression& e1, ValueExpression& e2, const vector<Value>& vals) const -> Value {
    return fn_(e1.eval(vals), e2.eval(vals));
}

auto UnaryArithmeticOperator::eval(ValueExpression& e, const vector<Value>& vals) const -> Value {
    return fn_(e.eval(vals));
}

} // namespace selector