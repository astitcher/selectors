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
#include "SelectorEnv.h"
#include "SelectorValue.h"

#include <benchmark/benchmark.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using std::string;
using std::string_view;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

using namespace std::literals;

namespace selector::benchmarks {

// Test environment that provides values
class BenchmarkEnv : public selector::Env {
    unordered_map<string, selector::Value> values;
    vector<string> string_storage;  // Keep string views valid

public:
    void set(const string& key, const selector::Value& value) {
        values[key] = value;
    }

    void set(const string& key, string_view sv) {
        string_storage.emplace_back(sv);
        values[key] = selector::Value(string_view{string_storage.back()});
    }

    void set(const string& key, int64_t i) {
        values[key] = selector::Value(i);
    }

    void set(const string& key, double d) {
        values[key] = selector::Value(d);
    }

    void set(const string& key, bool b) {
        values[key] = selector::Value(b);
    }

    const selector::Value& value(string_view sv) const override {
        string key{sv};
        auto it = values.find(key);
        static constexpr selector::Value EMPTY{};
        return (it != values.end()) ? it->second : EMPTY;
    }
};

// Helper to create and evaluate expression
bool eval_expr(string_view expr_str, const BenchmarkEnv& env) {
    auto expr = selector::make_selector(expr_str);
    return selector::eval(*expr, env);
}

// Benchmark parsing performance
static void BM_Parse_Simple(benchmark::State& state) {
    const string expr = "A = 42";
    for (auto _ : state) {
        auto result = selector::make_selector(expr);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * expr.size());
}
BENCHMARK(BM_Parse_Simple);

static void BM_Parse_Complex(benchmark::State& state) {
    const string expr = "(A = 42 AND B = 'hello') OR (C > 100 AND D LIKE 'test%')";
    for (auto _ : state) {
        auto result = selector::make_selector(expr);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * expr.size());
}
BENCHMARK(BM_Parse_Complex);

static void BM_Parse_LongChain(benchmark::State& state) {
    string expr = "A1 = 1";
    for (int i = 2; i <= 20; ++i) {
        expr += " AND A" + std::to_string(i) + " = " + std::to_string(i);
    }
    for (auto _ : state) {
        auto result = selector::make_selector(expr);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * expr.size());
}
BENCHMARK(BM_Parse_LongChain);

// Benchmark evaluation - simple comparisons
static void BM_Eval_SimpleEquality(benchmark::State& state) {
    auto expr = selector::make_selector("A = 42");
    BenchmarkEnv env;
    env.set("A", int64_t(42));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_SimpleEquality);

static void BM_Eval_StringComparison(benchmark::State& state) {
    auto expr = selector::make_selector("name = 'test'");
    BenchmarkEnv env;
    env.set("name", "test"sv);
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_StringComparison);

static void BM_Eval_NumericComparison(benchmark::State& state) {
    auto expr = selector::make_selector("value > 100");
    BenchmarkEnv env;
    env.set("value", int64_t(150));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_NumericComparison);

// Benchmark logical operators
static void BM_Eval_AND_ShortCircuit(benchmark::State& state) {
    auto expr = selector::make_selector("A = 0 AND B = 42");
    BenchmarkEnv env;
    env.set("A", int64_t(1));  // First condition false, should short-circuit
    env.set("B", int64_t(42));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_AND_ShortCircuit);

static void BM_Eval_OR_ShortCircuit(benchmark::State& state) {
    auto expr = selector::make_selector("A = 1 OR B = 42");
    BenchmarkEnv env;
    env.set("A", int64_t(1));  // First condition true, should short-circuit
    env.set("B", int64_t(42));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_OR_ShortCircuit);

static void BM_Eval_AND_LongChain(benchmark::State& state) {
    string expr_str = "A1 = 1";
    BenchmarkEnv env;
    env.set("A1", int64_t(1));
    for (int i = 2; i <= 10; ++i) {
        expr_str += " AND A" + std::to_string(i) + " = " + std::to_string(i);
        env.set("A" + std::to_string(i), int64_t(i));
    }
    auto expr = selector::make_selector(expr_str);
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_AND_LongChain);

// Benchmark arithmetic operations
static void BM_Eval_Arithmetic(benchmark::State& state) {
    auto expr = selector::make_selector("(A + B) * C / D");
    BenchmarkEnv env;
    env.set("A", int64_t(10));
    env.set("B", int64_t(20));
    env.set("C", int64_t(3));
    env.set("D", int64_t(2));
    
    for (auto _ : state) {
        auto result = expr->eval(env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_Arithmetic);

// Benchmark IS NULL operations
static void BM_Eval_IsNull(benchmark::State& state) {
    auto expr = selector::make_selector("A is null");
    BenchmarkEnv env;
    // A is not set, so it's null
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_IsNull);

static void BM_Eval_IsNotNull(benchmark::State& state) {
    auto expr = selector::make_selector("A is not null");
    BenchmarkEnv env;
    env.set("A", int64_t(42));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_IsNotNull);

// Benchmark LIKE expressions
static void BM_Eval_LIKE_Simple(benchmark::State& state) {
    auto expr = selector::make_selector("name LIKE 'test%'");
    BenchmarkEnv env;
    env.set("name", "testing"sv);
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_LIKE_Simple);

static void BM_Eval_LIKE_Complex(benchmark::State& state) {
    auto expr = selector::make_selector("name LIKE 'test_%pattern%'");
    BenchmarkEnv env;
    env.set("name", "test123pattern456"sv);
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_LIKE_Complex);

static void BM_Eval_LIKE_NoMatch(benchmark::State& state) {
    auto expr = selector::make_selector("name LIKE 'nomatch%'");
    BenchmarkEnv env;
    env.set("name", "testing"sv);
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_LIKE_NoMatch);

// Benchmark BETWEEN expressions
static void BM_Eval_BETWEEN(benchmark::State& state) {
    auto expr = selector::make_selector("value BETWEEN 10 AND 100");
    BenchmarkEnv env;
    env.set("value", int64_t(50));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_BETWEEN);

// Benchmark IN expressions
static void BM_Eval_IN_Small(benchmark::State& state) {
    auto expr = selector::make_selector("value IN (1, 2, 3, 4, 5)");
    BenchmarkEnv env;
    env.set("value", int64_t(3));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_IN_Small);

static void BM_Eval_IN_Large(benchmark::State& state) {
    string expr_str = "value IN (";
    for (int i = 1; i <= 50; ++i) {
        if (i > 1) expr_str += ", ";
        expr_str += std::to_string(i);
    }
    expr_str += ")";
    auto expr = selector::make_selector(expr_str);
    BenchmarkEnv env;
    env.set("value", int64_t(25));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_IN_Large);

static void BM_Eval_IN_NoMatch(benchmark::State& state) {
    auto expr = selector::make_selector("value IN (1, 2, 3, 4, 5)");
    BenchmarkEnv env;
    env.set("value", int64_t(99));  // Not in list
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_IN_NoMatch);

// Benchmark complex nested expressions
static void BM_Eval_ComplexNested(benchmark::State& state) {
    auto expr = selector::make_selector(
        "(A = 1 AND B = 2) OR (C > 100 AND D < 50) OR (E LIKE 'test%' AND F is not null)"
    );
    BenchmarkEnv env;
    env.set("A", int64_t(1));
    env.set("B", int64_t(2));
    env.set("C", int64_t(150));
    env.set("D", int64_t(30));
    env.set("E", "testing"sv);
    env.set("F", int64_t(42));
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_ComplexNested);

// Benchmark repeated evaluation (same expression, different environments)
static void BM_Eval_Repeated_SameExpr(benchmark::State& state) {
    auto expr = selector::make_selector("A = 42 AND B = 'test'");
    
    vector<BenchmarkEnv> envs;
    for (int i = 0; i < 100; ++i) {
        BenchmarkEnv env;
        env.set("A", int64_t(42));
        env.set("B", "test"sv);
        envs.push_back(std::move(env));
    }
    
    size_t env_idx = 0;
    for (auto _ : state) {
        bool result = selector::eval(*expr, envs[env_idx % envs.size()]);
        benchmark::DoNotOptimize(result);
        ++env_idx;
    }
}
BENCHMARK(BM_Eval_Repeated_SameExpr);

// Benchmark environment lookup performance
static void BM_Env_Lookup(benchmark::State& state) {
    BenchmarkEnv env;
    env.set("var1", int64_t(1));
    env.set("var2", int64_t(2));
    env.set("var3", int64_t(3));
    env.set("var4", int64_t(4));
    env.set("var5", int64_t(5));
    
    for (auto _ : state) {
        selector::Value value = env.value("var3");
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_Env_Lookup);

static void BM_Env_Lookup_Missing(benchmark::State& state) {
    BenchmarkEnv env;
    env.set("var1", int64_t(1));
    
    for (auto _ : state) {
        selector::Value value = env.value("missing_var");
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_Env_Lookup_Missing);

// Benchmark parse + eval together (typical use case)
static void BM_ParseAndEval(benchmark::State& state) {
    const string expr_str = "A = 42 AND B = 'test'";
    BenchmarkEnv env;
    env.set("A", int64_t(42));
    env.set("B", "test"sv);
    
    for (auto _ : state) {
        auto expr = selector::make_selector(expr_str);
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * expr_str.size());
}
BENCHMARK(BM_ParseAndEval);

// Benchmark with many variables in environment
static void BM_Eval_ManyVariables(benchmark::State& state) {
    auto expr = selector::make_selector("var50 = 50");
    BenchmarkEnv env;
    // Create 100 variables, only use one
    for (int i = 1; i <= 100; ++i) {
        env.set("var" + std::to_string(i), int64_t(i));
    }
    
    for (auto _ : state) {
        bool result = selector::eval(*expr, env);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Eval_ManyVariables);

} // namespace selector::benchmarks

BENCHMARK_MAIN();
