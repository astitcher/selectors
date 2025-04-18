#ifndef SELECTOR_EXPRESSION_H
#define SELECTOR_EXPRESSION_H

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

#include <iosfwd>
#include <memory>
#include <string_view>

#include "selectors_export.h"

namespace selector {

class Env;

class Expression {
public:
    virtual ~Expression() noexcept = 0;
    virtual auto repr(std::ostream&) const -> void = 0;
    virtual auto eval(const Env&) const -> Value = 0;
    virtual auto eval_bool(const Env&) const -> BoolOrNone = 0;
};

SELECTORS_EXPORT auto make_selector(std::string_view exp) -> std::unique_ptr<Expression>;
SELECTORS_EXPORT auto eval(const Expression&, const Env&) -> bool;
SELECTORS_EXPORT auto operator<<(std::ostream&, const Expression&) -> std::ostream&;
}

#endif
