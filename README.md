Geometric Hash Table
====================

Dense, flat, memory efficient, cache friendly, super fast Hash table targeting higher Cache-hit rates.

Having given that Hash function is collision-free, implementation is heavily optimized for Lookups.

## Unit tests

Build and run `GeometricHashTable.Tests` in Debug or Release. The executable covers parsing, point and triangle comparisons, hashing invariants, STL construction, lookup, collisions, and reconstruction behavior. Deterministic grid-based checks exercise all six vertex permutations and bucket bounds. Tests also make a current limitation explicit: points considered equal within `Point3::EPS` can quantize to different hash buckets, so `Lookup` can miss a geometrically equal triangle at a bucket boundary. This behavior is recorded rather than changed in the base implementation.

```powershell
msbuild GeometricHashTable.slnx /m /p:Configuration=Release /p:Platform=x64
.\x64\Release\GeometricHashTable.Tests.exe
```

## Benchmarking

Run `GeometricHashTable.Benchmarks` in Release mode for construction, lookup, and hash timings:

```text
GeometricHashTable.Benchmarks.exe [stl-file] [construction-runs] [lookup-iterations]
```

Defaults are `wavy_surface_100000.stl`, 5 construction runs, and 1,000,000 lookup iterations. The suite reports 250k/1m/4m-bucket construction scaling, known/reordered/missing lookups, a mixed lookup workload, and hash microbenchmarks. Lookup checksums are validated against fixed expectations or an independently built first-slot reference model; mismatches fail the process. One-shot timings are labeled as observations, not medians. Benchmark timings remain diagnostic and are not performance thresholds.

## Code coverage

Run `Run-Coverage.ps1` to build and execute the unit test program under Visual Studio Code Coverage. Coverage uses an unoptimized Debug build so `Construct`, `_Initialize`, and `_GetTriangle` are present in the report. The script requires those methods and all executable base-code sources, enforces 90% line and 95% block coverage overall, and requires 90% line and block coverage per source by default. Thresholds can be adjusted with the corresponding `-Minimum...Coverage` parameters when intentionally changing the gate. The full Visual Studio XML report is saved to `coverage.xml` by default.

```powershell
.\Run-Coverage.ps1
```

GitHub Actions builds the x64 Release solution, runs the unit tests and a short benchmark smoke test, collects and gates coverage, and uploads the report as a workflow artifact. Coverage requires Visual Studio with the C++ build tools and Microsoft Code Coverage Console installed.
