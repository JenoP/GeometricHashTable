# Geometric Hash Table

A dense, flat C++20 hash table for finding triangles in ASCII STL meshes. Triangles are normalized into vertex order and assigned to one bucket using a deterministic, tolerance-aware 64-bit hash.

The table stores at most one triangle per bucket. This favors fast construction and direct lookup; it does not provide collision chaining. In a hash collision, the first triangle stored in that bucket is retained.

## Projects

| Project | Purpose |
|---|---|
| `GeometricHashTable` | STL loader, triangle lookup, and command-line executable |
| `GeometricHashTable.Tests` | GoogleTest suite covering parsing, hashing, construction, lookup, and edge cases |
| `GeometricHashTable.Benchmarks` | Construction, lookup, initialization, hash, and distribution benchmarks |
| `GeometricHashTable.Coverage` | Instrumented test executable for Visual Studio Code Coverage |

## Implementation

- `Triangle.h` defines 3D points and triangles. Points compare with a `1e-6` absolute epsilon; triangles sort their vertices to make vertex order irrelevant.
- `Hash.h` quantizes coordinates, mixes vertex hashes with SplitMix64, sorts mixed vertex hashes, and reduces the result into the requested table size.
- `HashTable.h/.cpp` parses ASCII STL facet lines, stores `Triangle` values in `std::vector<std::optional<Triangle>>`, and performs direct bucket lookup.
- `Construct()` can be called repeatedly; successful additions remain in the table, and a missing file or malformed facet throws without clearing existing entries.

## C++ API

```cpp
#include "Triangle.h"
#include "HashTable.h"

HashTable table(1'000'000);
table.Construct("GeometricHashTable/wavy_surface_100000.stl");

Triangle triangle(
	Point3{ 0.0, 0.0, 0.0 },
	Point3{ 1.0, 0.0, 0.6661729 },
	Point3{ 1.0, 1.0, 0.6646931 });

bool exists = table.Lookup(triangle);
```

Include both headers in consuming code. The table size and STL path define construction and lookup behavior; exceptions indicate an unopened file or malformed vertex data.

## Build

Requirements:

- Visual Studio with the MSVC C++ workload and C++20 support
- x64 build tools (`v145` projects)
- NuGet restore support
- Microsoft Code Coverage Console for coverage collection

```powershell
nuget restore packages.config -PackagesDirectory packages
msbuild GeometricHashTable.slnx /m /p:Configuration=Release /p:Platform=x64
```

## Command-line executable

```text
GeometricHashTable.exe <stl-file> <triangle>
```

The triangle argument uses three brace-delimited 3D points:

```powershell
.\x64\Release\GeometricHashTable.exe `
  .\GeometricHashTable\wavy_surface_100000.stl `
  "{{0.0,0.0,0.0}{1.0,0.0,0.6661729}{1.0,1.0,0.6646931}}"
```

The executable prints construction and lookup timings plus whether the triangle exists.

## Tests

Run the 60-test GoogleTest executable directly:

```powershell
.\x64\Release\GeometricHashTable.Tests.exe
```

Coverage includes STL parsing, positive, negative, scientific and CRLF input, point/triangle equality and ordering, all six vertex permutations, hash bounds and stability, empty and degenerate triangles, duplicate and colliding facets, repeated construction, malformed input, and the included 100,000-facet wavy-surface fixture.

## Benchmarks

```text
GeometricHashTable.Benchmarks.exe [stl-file] [construction-runs] [lookup-iterations]
```

Defaults: `wavy_surface_100000.stl`, 5 construction runs, and 1,000,000 lookup iterations.

```powershell
.\x64\Release\GeometricHashTable.Benchmarks.exe `
  .\GeometricHashTable\wavy_surface_100000.stl 5 1000000
```

The benchmark suite measures 250k/1m/4m-bucket construction scaling, slot initialization strategies, known/reordered/missing lookups, tolerance and collision cases, mixed workloads, randomized checksums, SplitMix64 operations, and hash distributions. It validates lookup checksums and fails on mismatch; reported timings are observations, not pass/fail thresholds.

## Code coverage

Coverage collection uses an unoptimized Debug build and Microsoft Code Coverage Console:

```powershell
.\Run-Coverage.ps1
```

It writes `coverage.xml` by default and checks:

- Overall: at least 90% line coverage and 95% block coverage
- Each base source file: at least 90% line and block coverage
- Required instrumented functions: `Construct`, `_Initialize`, `_ConstructTriangle`, and `Lookup`

Thresholds can be overridden with `-MinimumLineCoverage`, `-MinimumBlockCoverage`, `-MinimumSourceLineCoverage`, and `-MinimumSourceBlockCoverage`.

## Continuous integration

The GitHub Actions workflow builds the solution for x64 Release, restores NuGet packages, runs all unit tests, runs a short benchmark smoke test, collects gated Debug coverage, and uploads `coverage.xml` as an artifact.

## Current limitations

- Only ASCII STL files are supported.
- The table retains one primary triangle per bucket; later colliding triangles are discarded.
- Lookup is tolerance-based for points but hashes coordinates by quantization. In rare bucket-boundary cases, coordinate differences within the epsilon can quantize differently and prevent a lookup even if point equality would succeed.
