# Geometric Hash Table

A high-performance C++20 geometric lookup table for ASCII STL meshes. The implementation normalizes every triangle into a deterministic vertex order and maps it to a dense, fixed-size bucket using a tolerance-aware 64-bit hash.

The table stores at most one triangle per bucket. It is intentionally a flat lookup structure rather than a chained hash table: construction and lookup remain cache-friendly, while a hash collision retains only the first triangle assigned to that bucket.

## Project layout

| Project | Role |
|---|---|
| `GeometricHashTable` | Base library, STL construction and triangle lookup implementation, and command-line executable |
| `GeometricHashTable.Tests` | GoogleTest suite for parsing, hashing, normalization, reconstruction, collision, and lookup behavior |
| `GeometricHashTable.Benchmarks` | Release-oriented construction, lookup, initialization, hash, and distribution benchmarks |
| `GeometricHashTable.Coverage` | Instrumented test executable for Microsoft Visual Studio Code Coverage |

## How it works

1. **Parse:** `HashTable::Construct()` reads ASCII STL text and processes each `outer loop` containing three `vertex` records. `GetVertex()` parses each `vertex` record into an existing `Point3&` output.
2. **Normalize:** `Triangle::Normalize()` sorts the three points lexicographically by X, Y, then Z, making vertex permutations equivalent.
3. **Quantize:** `Hash.h` converts coordinates to integer cells using a configurable tolerance.
4. **Mix:** Each point is hashed with `mix64()`, a SplitMix64 finalizer. For normal lookups, triangle vertex hashes are sorted before the final mixing step.
5. **Store:** `_ConstructTriangle()` computes a bucket and stores one `Triangle` in `std::vector<std::optional<Triangle>>`.
6. **Lookup:** `Lookup()` computes the same bucket and compares the stored triangle with the query using `Point3`'s `1e-6` absolute-coordinate equality.

Primary source files:

- `GeometricHashTable/Triangle.h` - points, triangle ordering and STL vertex parsing
- `GeometricHashTable/Hash.h` - quantization, mixing and bucket reduction
- `GeometricHashTable/HashTable.h/.cpp` - STL construction and direct bucket lookup
- `GeometricHashTable/main.cpp` - command-line interface

## Requirements

- Windows
- Visual Studio 2022 or later with Desktop development with C++ and C++20 support
- MSVC `v145` build tools
- NuGet for package restore
- Microsoft Code Coverage Console for gated coverage collection

The repository includes GoogleTest as a package under `packages/`.

## Build

From a Developer PowerShell or the repository root:

```powershell
nuget restore packages.config -PackagesDirectory packages
msbuild GeometricHashTable.slnx /m /p:Configuration=Release /p:Platform=x64
```

All four Visual Studio projects are configured for Win32 and x64. The commands below use x64 and Release because those outputs are selected by the workflow and test runners.

## C++ API

```cpp
#include "GeometricHashTable/Triangle.h"
#include "GeometricHashTable/HashTable.h"

HashTable table(1'000'000);
table.Construct("GeometricHashTable/wavy_surface_100000.stl");

Triangle triangle(
	Point3{ 0.0, 0.0, 0.0 },
	Point3{ 1.0, 0.0, 0.6661729 },
	Point3{ 1.0, 1.0, 0.6646931 });

bool exists = table.Lookup(triangle);

Point3 parsedVertex;
GetVertex("vertex 1.25 -2.5 3.75", parsedVertex);
// parsedVertex == Point3{ 1.25, -2.5, 3.75 }
```

`GetVertex()` uses a reference output parameter and is not nullable. Callers must provide a valid `Point3&`.

### Construction behavior

- A table of size `0` has no storage; construction succeeds for an empty STL.
- `Construct()` resizes storage to the configured table size.
- Repeated construction is additive for successfully parsed facets.
- Reconstructing the same facet is safe and does not change lookup results.
- An unopened file throws `std::invalid_argument`.
- Malformed or incomplete vertex/facet data throws `std::invalid_argument` or `std::runtime_error` from parsing helpers.
- A malformed reconstruction does not explicitly clear an earlier successful table state.

## Command-line executable

```text
GeometricHashTable.exe <stl-file> <triangle>
```

The triangle argument contains three brace-delimited points. Quote the argument in PowerShell:

```powershell
.\x64\Release\GeometricHashTable.exe `
  .\GeometricHashTable\wavy_surface_100000.stl `
  "{{0.0,0.0,0.0}{1.0,0.0,0.6661729}{1.0,1.0,0.6646931}}"
```

The executable reports STL construction time, whether the triangle exists, and lookup time.

## Unit tests

Run the GoogleTest executable directly:

```powershell
.\x64\Release\GeometricHashTable.Tests.exe
```

It is also available as a Visual Studio Test Explorer project. The suite now contains 69 tests across point, triangle, parser, hash, and hash-table behavior.

The tests cover, among other cases:

- positive, negative, scientific, CRLF, and whitespace-formatted coordinates
- malformed, missing, overflow, empty, and incomplete vertex input
- reference-based `GetVertex()` parsing for positive, negative, scientific, trailing, and non-finite coordinate input
- point equality and lexicographic ordering
- NaN-characterized behavior and negative-zero quantization
- every triangle vertex permutation
- winding-sensitive and winding-independent hash variants
- custom tolerances and table-size bounds
- empty, degenerate, duplicate, and colliding facets
- repeated and failed reconstruction
- known and missing triangles in the included 100,000-facet STL fixture

## Benchmarks

```text
GeometricHashTable.Benchmarks.exe [stl-file] [construction-runs] [lookup-iterations]
```

Defaults:

- STL: `GeometricHashTable/wavy_surface_100000.stl`
- Construction runs: `5`
- Lookup iterations: `1,000,000`

Run the benchmark suite with custom values:

```powershell
.\x64\Release\GeometricHashTable.Benchmarks.exe `
  .\GeometricHashTable\wavy_surface_100000.stl 5 1000000
```

Use a short smoke workload locally or in CI:

```powershell
.\x64\Release\GeometricHashTable.Benchmarks.exe `
  .\GeometricHashTable\wavy_surface_100000.stl 1 32
```

Measured groups include:

- construction at 250k, 1m, and 4m buckets
- serial, parallel, and bulk slot initialization
- known, reordered, missing, tolerance, collision, and mixed lookups
- randomized lookup checksum workloads
- order-independent and winding-sensitive triangle hashes
- point, quantization, mixer, and reference-based `GetVertex()` microbenchmarks
- hash distribution across 4096 buckets
- tolerance and triangle-normalization microbenchmarks

Benchmark checksum mismatches fail the executable. Timings are observations; they are not asserted performance thresholds.

## Code coverage

Collect Microsoft Visual Studio Code Coverage from an unoptimized Debug build:

```powershell
.\Run-Coverage.ps1
```

By default, the report is written to `coverage.xml`. The script also verifies:

- overall coverage of at least 90% lines and 95% blocks
- at least 90% line and block coverage for each instrumented base source file
- instrumented `HashTable` functions: `Construct`, `_Initialize`, `_ConstructTriangle`, and `Lookup`

The thresholds are configurable:

```powershell
.\Run-Coverage.ps1 `
  -MinimumLineCoverage 90 `
  -MinimumBlockCoverage 95 `
  -MinimumSourceLineCoverage 90 `
  -MinimumSourceBlockCoverage 90
```

A recent Debug coverage run reported:

- **96.88%** overall lines
- **97.95%** overall blocks
- **100%** lines and blocks for `Hash.h`
- **69/69** unit tests passing

Coverage totals can vary with compiler and test-environment versions, so use the generated XML and script gate as the authoritative result.

## Continuous integration

`.github/workflows/msbuild.yml` runs on pushes and pull requests to `master`. It:

1. restores NuGet packages;
2. builds the solution for x64 Release;
3. runs all unit tests;
4. runs a short benchmark smoke workload with 32 lookup iterations;
5. rebuilds and runs an instrumented Debug coverage executable containing 69 shared unit tests;
6. enforces the coverage thresholds; and
7. uploads `coverage.xml` as `geometric-hash-table-coverage`.

## Data and collision limitations

- Only ASCII STL files are supported; binary STL input is not handled.
- Construction scans `outer loop` blocks and ignores unrelated solid/facet text.
- Only the first triangle placed in a bucket is retained.
- `HashTable::Lookup()` is exact for the stored normalized triangle and uses the point epsilon for equality.
- Quantization and hash bucket reduction are separate operations. A coordinate difference within the equality epsilon can occasionally cross a quantization bucket boundary, so the lookup is not guaranteed to match every epsilon-equal triangle.
- Results depend on a selected table size: larger tables reduce collision pressure, but reserve a dense optional-triangle slot for every bucket.

## Repository conventions

- Keep new unit tests in `GeometricHashTable.Tests/HashTable.GoogleTests.cpp`.
- Include behavioral additions in `GeometricHashTable.Coverage/GeometricHashTable.Coverage.vcxproj` through the shared test source file.
- Keep benchmark-only workloads in `GeometricHashTable.Benchmarks/Benchmarks.cpp`.
- Do not use benchmark timings as correctness assertions; use checksum and result validation for benchmark correctness.
