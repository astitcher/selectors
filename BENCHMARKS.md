# Selector Benchmarks

This document describes the benchmark suite for the selector expression evaluator.

## Building Benchmarks

### Prerequisites

Google Benchmark must be installed and available to CMake via `find_package`. The build system will automatically detect it if installed.

**Installing Google Benchmark:**

On Debian/Ubuntu:
```bash
sudo apt-get install libbenchmark-dev
```

On Fedora/RHEL:
```bash
sudo dnf install benchmark-devel
```

On macOS (Homebrew):
```bash
brew install google-benchmark
```

Or build from source:
```bash
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -Bbuild -S. -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Building

If Google Benchmark is found, benchmarks will be automatically enabled:

```bash
mkdir build
cd build
cmake ..
make selector_benchmarks
```

If Google Benchmark is not found, CMake will print a status message:
```
-- Google Benchmark not found - benchmarks disabled
```

In this case, install Google Benchmark and reconfigure CMake.

**Verifying benchmarks are enabled:**

After running `cmake ..`, check that the `selector_benchmarks` target exists:
```bash
cmake --build . --target help | grep selector_benchmarks
```

If benchmarks are enabled, you should see `selector_benchmarks` in the list of available targets.

## Running Benchmarks

### Basic Usage

Run all benchmarks:

```bash
./selector_benchmarks
```

Or use the CMake target:

```bash
make run_benchmarks
```

### Advanced Options

Google Benchmark provides many options. Common ones:

```bash
# Run specific benchmark(s) by name pattern
./selector_benchmarks --benchmark_filter="BM_Eval.*"

# Set minimum time per benchmark (seconds)
./selector_benchmarks --benchmark_min_time=2.0

# Set number of iterations
./selector_benchmarks --benchmark_iterations=1000

# Output JSON for analysis
./selector_benchmarks --benchmark_format=json > results.json

# Compare with previous run
./selector_benchmarks --benchmark_format=json > baseline.json
# ... make changes ...
./selector_benchmarks --benchmark_format=json > current.json
# Compare using benchmark tools or manually
```

### Performance Analysis

For detailed performance analysis:

```bash
# Run with CPU profiling
perf record ./selector_benchmarks
perf report

# Or use Google Benchmark's built-in statistics
./selector_benchmarks --benchmark_report_aggregates_only=true
```

## Benchmark Categories

The suite includes benchmarks for:

### Parsing Performance
- `BM_Parse_Simple` - Simple expression parsing
- `BM_Parse_Complex` - Complex nested expression parsing
- `BM_Parse_LongChain` - Long AND chain parsing

### Evaluation Performance
- **Simple Operations**: Equality, string comparison, numeric comparison
- **Logical Operators**: AND/OR with short-circuiting, long chains
- **Arithmetic**: Basic arithmetic operations
- **Special Operators**: IS NULL, LIKE, BETWEEN, IN
- **Complex Expressions**: Nested logical expressions

### Environment Lookup
- `BM_Env_Lookup` - Variable lookup performance
- `BM_Env_Lookup_Missing` - Missing variable lookup

### Real-World Scenarios
- `BM_ParseAndEval` - Combined parse + eval (typical use case)
- `BM_Eval_Repeated_SameExpr` - Repeated evaluation with same expression
- `BM_Eval_ManyVariables` - Evaluation with large environment

## Interpreting Results

Benchmark output shows:
- **Time**: Average time per iteration (lower is better)
- **CPU**: CPU time used
- **Iterations**: Number of iterations run
- **Bytes Processed**: For parsing benchmarks, bytes of input processed

Example output:
```
BM_Eval_SimpleEquality         1000000    125 ns    125 ns  5600000
```

This means:
- 1,000,000 iterations
- 125 nanoseconds per iteration
- 5,600,000 iterations/second throughput

## Using Results for Optimization

1. **Baseline**: Run benchmarks before making changes
2. **After Changes**: Run again and compare
3. **Focus Areas**: Identify slow benchmarks from the optimization evaluation document
4. **Validate**: Ensure optimizations improve the right benchmarks

## Tips

- Run benchmarks multiple times and average results
- Use release builds (`-DCMAKE_BUILD_TYPE=Release`)
- Disable CPU frequency scaling for consistent results
- Close other applications to reduce interference
- Use `--benchmark_min_time` for more stable results
