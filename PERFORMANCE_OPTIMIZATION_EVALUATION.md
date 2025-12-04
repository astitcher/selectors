# Performance Optimization Evaluation for Selector Expression Evaluator

## Executive Summary

This document evaluates various approaches to speed up expression evaluation in the selector library. The current implementation uses a tree-based AST with virtual function dispatch, which provides flexibility but has performance overhead.

## Current Architecture Analysis

### Implementation Characteristics

1. **AST Structure**: Tree-based with polymorphic nodes (`unique_ptr<Expression>`)
2. **Evaluation Model**: Virtual function dispatch (`eval()`, `eval_bool()`)
3. **Value Representation**: `std::variant` for type-safe value storage
4. **Environment Lookup**: `unordered_map<string_view, unique_ptr<Value>>`
5. **Memory Model**: Heap-allocated expression tree nodes

### Identified Performance Bottlenecks

1. **Virtual Function Calls**: Every node evaluation requires indirect calls
2. **Heap Allocations**: Expression tree nodes allocated on heap
3. **Value Copying**: `Value` objects passed by value in many operations
4. **Environment Lookups**: String-to-string conversion in C interface (line 54 in selectors.cpp)
5. **Variant Overhead**: Type checking via `std::variant::index()` and `std::visit`
6. **Regex Performance**: `std::regex` for LIKE expressions (can be slow)
7. **IN Expression**: Evaluates all list items even when early match possible (though does return early)

## Optimization Approaches

### 1. Template-Based Evaluation (Compile-Time Dispatch)

**Approach**: Replace virtual functions with template-based evaluation using `std::variant` visitation or tag dispatch.

**Implementation Complexity**: Medium-High
- Requires refactoring expression hierarchy
- Need to maintain type erasure for C API
- Template metaprogramming complexity

**Expected Performance Gain**: 20-40% faster evaluation
- Eliminates virtual function call overhead
- Better compiler optimization opportunities
- Inlining of evaluation logic

**Pros**:
- Significant performance improvement
- Maintains type safety
- Better compiler optimization

**Cons**:
- Increased compile time
- More complex code
- Harder to debug
- May increase binary size

**Recommendation**: High priority for hot paths

---

### 2. Bytecode/VM Approach

**Approach**: Compile AST to bytecode, execute in simple stack-based VM.

**Implementation Complexity**: High
- Need bytecode instruction set design
- Compiler from AST to bytecode
- VM interpreter implementation
- Debugging/decompilation tools

**Expected Performance Gain**: 30-60% faster for repeated evaluations
- Better instruction cache locality
- Simpler evaluation loop
- Can optimize bytecode sequences

**Pros**:
- Excellent for repeated evaluations (same expression, different envs)
- Better cache locality
- Can add JIT compilation later
- Easier to profile and optimize

**Cons**:
- High initial implementation cost
- One-time compilation overhead
- More complex debugging
- Memory overhead for bytecode storage

**Recommendation**: High priority if expressions are reused frequently

---

### 3. Expression Tree Flattening

**Approach**: Flatten binary trees (AND/OR chains) into arrays, evaluate in loops.

**Implementation Complexity**: Medium
- Modify parser to build flat structures
- Update evaluation logic

**Expected Performance Gain**: 10-25% for long chains
- Reduces tree traversal overhead
- Better cache locality
- Simpler evaluation loops

**Pros**:
- Moderate performance gain
- Relatively simple implementation
- Maintains current architecture

**Cons**:
- Limited to specific expression patterns
- May complicate other optimizations

**Recommendation**: Medium priority, good incremental improvement

---

### 4. Environment Lookup Optimization

**Approach**: 
- Use interned strings consistently (already partially done)
- Optimize hash function for common identifiers
- Cache frequently accessed values
- Use `string` keys instead of `string_view` in map (avoid conversion)

**Implementation Complexity**: Low-Medium
- Fix string conversion in `selector_environment_t::value()` (line 54)
- Implement string interning for all identifiers
- Add value caching if needed

**Expected Performance Gain**: 5-15% for expression-heavy workloads
- Eliminates string allocation in lookups
- Better hash performance
- Reduced memory allocations

**Pros**:
- Low risk, high reward
- Easy to implement
- Immediate benefit

**Cons**:
- Limited to identifier-heavy expressions
- String interning has memory cost

**Recommendation**: **High priority - quick win**

**Specific Fix**: In `selectors.cpp:54`, change:
```cpp
if (auto i = values.find(string{sv}); i!=values.end()) {
```
to use interned strings or `string` keys directly.

---

### 5. Value Type Specialization

**Approach**: Use specialized value types instead of `std::variant` for common cases, or optimize variant access patterns.

**Implementation Complexity**: Medium
- Create specialized value classes
- Update all value operations
- Maintain compatibility

**Expected Performance Gain**: 10-20%
- Eliminates variant overhead
- Better type-specific optimizations
- Reduced memory overhead

**Pros**:
- Significant improvement for value-heavy operations
- Better type safety

**Cons**:
- Requires extensive refactoring
- May complicate codebase

**Alternative**: Optimize variant access:
- Use `std::get_if` instead of `std::visit` where possible
- Cache type checks
- Use `std::variant` with custom accessors

**Recommendation**: Medium priority, consider variant optimization first

---

### 6. Short-Circuit Optimization Enhancement

**Approach**: Current AND/OR already short-circuit, but can optimize further:
- Reorder expressions by estimated cost
- Early exit for known-false/true patterns
- Constant folding during parsing

**Implementation Complexity**: Low-Medium
- Expression reordering logic
- Constant folding in parser
- Cost estimation heuristics

**Expected Performance Gain**: 5-20% depending on expression patterns
- Skips expensive evaluations
- Better branch prediction

**Pros**:
- Low risk
- Works with current architecture
- Can combine with other optimizations

**Cons**:
- Requires cost estimation
- May not help all expressions

**Recommendation**: Medium priority

---

### 7. LIKE Expression Optimization

**Approach**: 
- Replace `std::regex` with custom pattern matcher for simple patterns
- Cache compiled regex objects
- Use string matching for patterns without wildcards

**Implementation Complexity**: Medium
- Implement simple pattern matcher
- Pattern analysis logic
- Fallback to regex for complex patterns

**Expected Performance Gain**: 50-90% for LIKE expressions
- `std::regex` is notoriously slow
- Simple patterns can use fast string matching

**Pros**:
- Huge win for LIKE-heavy workloads
- Maintains compatibility

**Cons**:
- Only helps LIKE expressions
- Need to handle edge cases

**Recommendation**: **High priority if LIKE is used frequently**

---

### 8. IN Expression Optimization

**Approach**: 
- Pre-evaluate constant list items during parsing
- Use hash set for large IN lists
- Sort list for binary search on ordered types

**Implementation Complexity**: Medium
- Constant detection in parser
- Hash set implementation
- Sorting logic

**Expected Performance Gain**: 20-80% for large IN lists
- O(1) lookup vs O(n) linear search
- Skips evaluation of constant expressions

**Pros**:
- Significant improvement for large lists
- Maintains semantics

**Cons**:
- Only helps IN expressions
- Memory overhead for hash sets

**Recommendation**: Medium priority, high value for large IN lists

---

### 9. Value Result Caching/Memoization

**Approach**: Cache evaluation results for sub-expressions that don't depend on environment.

**Implementation Complexity**: Medium-High
- Dependency analysis
- Cache invalidation logic
- Memory management

**Expected Performance Gain**: 0-50% depending on expression reuse
- Only helps if same expression evaluated multiple times
- Complex to implement correctly

**Pros**:
- Can provide significant gains for repeated patterns

**Cons**:
- Complex implementation
- Memory overhead
- May not help single-use expressions

**Recommendation**: Low priority unless profiling shows benefit

---

### 10. JIT Compilation

**Approach**: Compile expressions to native code at runtime (using LLVM, libjit, or similar).

**Implementation Complexity**: Very High
- JIT compiler integration
- Code generation
- Runtime linking
- Platform-specific code

**Expected Performance Gain**: 2-10x for hot expressions
- Native code execution
- Maximum optimization

**Pros**:
- Best possible performance
- Can optimize for specific environments

**Cons**:
- Very high implementation cost
- Platform dependencies
- Security concerns (code generation)
- Maintenance burden

**Recommendation**: Low priority unless performance is critical and other optimizations insufficient

---

### 11. SIMD Optimization

**Approach**: Use SIMD instructions for bulk operations (e.g., multiple comparisons in IN lists).

**Implementation Complexity**: High
- Platform-specific code
- SIMD instruction knowledge
- Fallback implementations

**Expected Performance Gain**: 2-4x for vectorizable operations
- Parallel processing
- Only helps specific patterns

**Pros**:
- Significant speedup for applicable cases

**Cons**:
- Limited applicability
- Platform-specific
- Complex implementation

**Recommendation**: Low priority, consider only if profiling shows vectorizable hotspots

---

### 12. Memory Pool Allocation

**Approach**: Use memory pools for expression tree nodes instead of individual heap allocations.

**Implementation Complexity**: Low-Medium
- Implement or use memory pool
- Update allocation code
- Lifetime management

**Expected Performance Gain**: 5-15%
- Reduces allocation overhead
- Better cache locality
- Faster deallocation

**Pros**:
- Moderate improvement
- Relatively simple
- Works with current architecture

**Cons**:
- Memory overhead
- Lifetime management complexity

**Recommendation**: Medium priority if allocations are bottleneck

---

## Recommended Implementation Order

### Phase 1: Quick Wins (Low effort, good return)
1. **Environment Lookup Optimization** - Fix string conversion issue
2. **LIKE Expression Optimization** - Replace std::regex for simple patterns
3. **Expression Tree Flattening** - For AND/OR chains

### Phase 2: Medium Effort (Moderate complexity, significant gains)
4. **IN Expression Optimization** - Hash sets for large lists
5. **Value Type Optimization** - Optimize variant access patterns
6. **Short-Circuit Enhancement** - Constant folding, reordering

### Phase 3: High Effort (Complex but high impact)
7. **Template-Based Evaluation** - Replace virtual dispatch
8. **Bytecode/VM Approach** - For repeated evaluation scenarios

### Phase 4: Advanced (If needed)
9. **JIT Compilation** - Only if other optimizations insufficient
10. **SIMD Optimization** - For specific hotspots

## Measurement Strategy

Before implementing optimizations:
1. **Profile** with realistic workloads to identify actual bottlenecks
2. **Benchmark** current performance with representative expressions
3. **Measure** after each optimization to validate improvements
4. **Test** thoroughly to ensure correctness is maintained

## Conclusion

The highest-impact, lowest-risk optimizations are:
1. Fixing the environment lookup string conversion (immediate 5-15% gain)
2. Optimizing LIKE expressions (50-90% gain for LIKE-heavy workloads)
3. Template-based evaluation or bytecode approach for repeated evaluations (20-60% gain)

The choice between template-based evaluation and bytecode depends on usage patterns:
- **Template-based**: Better for one-off evaluations, simpler implementation
- **Bytecode**: Better for repeated evaluations of same expression, more complex but higher potential

Consider implementing Phase 1 optimizations first, then measure to guide Phase 2 decisions.
