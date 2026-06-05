# fin-kit

High-performance financial computing primitives for Go applications, implemented in C and exposed through cgo.

fin-kit targets CPU-intensive workloads in quantitative trading, cryptocurrency trading, exchange infrastructure, brokerage systems, risk management, market surveillance, and backtesting. It provides batch-oriented computation APIs so Go systems can keep Go's engineering productivity while using low-level C implementations for SIMD, memory alignment, and deterministic numerical performance.

## Scope

fin-kit is a computation library, not a full trading platform. It provides reusable financial and numerical primitives for:

- Market data processing and order book analytics
- Quantitative factor computation and statistical analysis
- Risk management, portfolio analytics, VaR/CVaR, and drawdown metrics
- Pricing and valuation primitives for derivatives and fixed-income workflows
- Trade execution analytics such as VWAP, TWAP, slippage, and TCA
- Strategy backtesting support for large historical datasets
- Cryptocurrency derivatives, AMM, funding, liquidation, and arbitrage calculations
- Exchange and brokerage infrastructure calculations, including auction pricing, order validation, clearing, settlement, and surveillance

It does not provide networking, database access, user interfaces, trading strategies, blockchain node operation, smart contract deployment, or complete exchange/business-system frameworks.

## Design Goals

- **Batch first**: APIs process arrays and large batches to amortize the fixed cgo call overhead, typically targeting at least 1,000 elements per call.
- **High performance**: Hot paths are implemented in C with SIMD-aware dispatch and cache-conscious algorithms.
- **Zero allocation on hot paths**: Callers own input, output, and temporary buffers. Complex routines use explicit workspace sizing where needed.
- **Deterministic numerical behavior**: Floating-point behavior follows IEEE 754 expectations, with documented handling for NaN and Inf.
- **Thread safety**: Public computation functions avoid mutable global state and are suitable for Go concurrency.
- **Modular architecture**: Core modules are independent repositories integrated as Git submodules under `core/`.
- **Stable C ABI**: Public C headers expose flat, explicit APIs with status-code based error handling.
- **Go integration**: Go bindings wrap cgo calls, memory ownership, and error mapping.

## Architecture

fin-kit is organized as layered components:

```text
Go applications
  Quant engines | Risk systems | Matching engines | Clearing systems | ...
        |
Go binding layer
  cgo wrappers, Go/C type conversion, memory lifecycle, error mapping
        |
C API layer
  stable public headers, input validation, error codes, SIMD dispatch
        |
Domain compute layer
  M11 crypto trading primitives | M12 exchange and brokerage primitives
        |
Core compute layer
  M01 matrix | M02 stats | M03 sort | M04 math | M05 random
  M06 codec  | M07 data structures | M08 hash | M09 string | M10 optim
        |
Platform abstraction layer
  CPU feature detection, SIMD selection, aligned memory, platform compatibility
```

Core modules are maintained as independent Git submodules. The main repository integrates them and provides domain-specific financial modules.

## Modules

| Module | Area | Examples |
| --- | --- | --- |
| M01 | Matrix and linear algebra | GEMM, GEMV, transpose, LU/QR/Cholesky/SVD, linear solves, vector dot/norm/distance |
| M02 | Statistics | mean, variance, covariance, correlation, quantiles, rolling statistics, EMA, Welford, z-score, rank, Kahan summation |
| M03 | Sorting | floating-point sort, integer radix sort, argsort, top-k, multi-key sorting |
| M04 | Math functions | batch exp/log/sqrt/pow, normal CDF/PDF/inverse CDF, erf, cumsum, cummax |
| M05 | Random generation | uniform and normal random numbers, correlated normal vectors, batch generation, reproducible seeds |
| M06 | Codec and compression | LZ4, Zstd, delta encoding, varint, fused time-series encoding, JSON/CSV parsing, binary protocol and FIX encoding |
| M07 | Data structures | Bloom filter, Roaring bitmap, ring buffer, fixed-size memory pool, arena allocator |
| M08 | Hash and checksum | xxHash, CRC32C, SHA-256, SimHash |
| M09 | String processing | SIMD substring search, Aho-Corasick multi-pattern matching, string-to-number conversion, batch comparison |
| M10 | Optimization | quadratic programming, least-squares regression, Newton-Raphson root solving |
| M11 | Crypto trading | mark price, funding rate, liquidation price, PnL, AMM calculations, slippage, impermanent loss, arbitrage, basis, portfolio margin, fixed-point arithmetic, volatility surfaces |
| M12 | Exchange and brokerage | call auction equilibrium price, batch order validation, order book aggregation, order priority sorting, self-trade detection, market data encoding, clearing netting, mark-to-market, fee calculation, surveillance, market making, smart order routing |

## SIMD and Runtime Dispatch

Performance-critical functions provide multiple implementations where applicable:

- AVX-512
- AVX2
- SSE4.2
- ARM NEON
- Scalar fallback

At initialization, the platform layer detects CPU capabilities and dispatches each supported function to the best available implementation. SIMD-specific source files are compiled with their own compiler flags so binaries can run safely on lower-end CPUs without illegal-instruction failures.

## Memory Model

fin-kit follows an explicit ownership model:

- Input and output buffers are allocated by the caller.
- Hot-path functions do not allocate heap memory.
- SIMD routines use aligned memory where needed.
- Complex algorithms use a two-step workspace pattern: query required workspace size, allocate once, then pass the workspace into the computation.
- Go helpers may provide aligned slices backed by C allocation for SIMD-friendly workloads.

## Repository Layout

```text
fin-kit/
├── Makefile                  # top-level Go build/test wrapper
├── README.md
├── core/                     # independent Git submodules
│   ├── platform/             # platform abstraction, SIMD detection, aligned memory
│   ├── matrix/               # M01 matrix and linear algebra
│   ├── stats/                # M02 statistics
│   ├── sort/                 # M03 sorting
│   ├── math/                 # M04 math functions
│   ├── random/               # M05 random generation
│   ├── codec/                # M06 codec and compression
│   ├── ds/                   # M07 data structures
│   ├── hash/                 # M08 hash and checksum
│   ├── string_ops/           # M09 string processing
│   ├── optim/                # M10 optimization
│   ├── crypto/               # M11 crypto trading primitives
│   └── exchange/             # M12 exchange and brokerage primitives
├── broker/     
├── exchange/
└── hft/               
```

Some modules may be introduced progressively. Each core module is intended to be independently buildable and testable.

## Build and Test

Initialize or update submodules:

```bash
make sync
```

Run Go tests:

```bash
make test
```

Clean build artifacts:

```bash
make clean
```

C library builds are handled by individual modules. To build or test a core module, enter the module directory and use its own Makefile:

```bash
cd core/<module>
make
make test
```

## API Conventions

C APIs use the naming format:

```text
fc_<module>_<operation>[_<variant>]
```

Common prefixes:

| Prefix | Module |
| --- | --- |
| `fc_mat_` | matrix and linear algebra |
| `fc_stat_` | statistics |
| `fc_sort_` | sorting |
| `fc_math_` | math functions |
| `fc_rand_` | random generation |
| `fc_codec_` | codec and compression |
| `fc_ds_` | data structures |
| `fc_hash_` | hash and checksum |
| `fc_str_` | string processing |
| `fc_opt_` | optimization |
| `fc_ct_` | crypto trading |
| `fc_ex_` | exchange and brokerage |

Public C functions use flat parameters such as pointers, lengths, primitive numeric types, and explicit output buffers. Errors are reported with status codes instead of `errno`.

## Platform Support

Primary production target:

- Linux x86_64

Development and compatibility targets:

- Windows x86_64
- macOS x86_64
- macOS ARM64
- Go 1.18+

CPU fallback behavior is part of the design: AVX-512-capable systems use AVX-512 implementations, while systems without newer instruction sets fall back to AVX2, SSE4.2, NEON, or scalar implementations as appropriate.

## Quality Expectations

The project is intended for financial-grade workloads. Implementations should provide:

- Input validation for public APIs
- Bounds checks and explicit error reporting
- Clear NaN/Inf behavior
- Reproducible results across runs
- No undefined behavior dependencies
- Independent unit tests and benchmarks for each module
- Static analysis, sanitizer, and cross-platform build coverage where applicable

## Status

fin-kit is organized as a modular, evolving high-performance computing library. The main repository integrates independent core modules and domain modules for financial applications. See individual module directories for the currently available APIs, tests, and implementation status.
