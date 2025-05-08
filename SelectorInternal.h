#ifndef SELECTOR_INTERNAL_H
#define SELECTOR_INTERNAL_H

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
#include "SelectorValue.h"

#include <ostream>
#include <type_traits>
#include <vector>

namespace selector {

// Define operator<< for classes that have repr()
// need to define type trait has_repr for those clases
template<class T>
struct has_repr{static constexpr bool value = false;};

template<class T>
inline constexpr bool has_repr_v = has_repr<T>::value;

template <class T>
auto inline operator<<(std::ostream& o, const T& t) -> std::enable_if_t<has_repr_v<T>, std::ostream&>
{
    t.repr(o);
    return o;
}

class ValueExpression {
public:
    virtual ~ValueExpression() noexcept = default;
    virtual auto repr(std::ostream&) const -> void = 0;
    virtual auto eval(const std::vector<Value>&) const -> Value = 0;
    auto eval_bool(const std::vector<Value>& vals) const -> BoolOrNone {
        return eval(vals);
    }
};

template <> struct has_repr<ValueExpression> {static constexpr bool value = true;};

} // namespace selector

#endif // SELECTOR_INTERNAL_H