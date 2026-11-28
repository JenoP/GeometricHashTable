#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../GeometricHashTable/Triangle.h"
#include "../GeometricHashTable/Hash.h"
#include "../GeometricHashTable/HashTable.h"

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::duration<double, std::nano>;

namespace {

constexpr std::uint32_t TABLE_SIZE = 1'000'000;

struct Summary
{
	double min;
	double mean;
	double median;
	double max;
};

struct BenchmarkOptions
{
	std::size_t constructionIterations = 5;
	std::size_t lookupIterations = 1'000'000;
	std::filesystem::path stlPath;
};

Summary Summarize(std::vector<double> samples)
{
	std::sort(samples.begin(), samples.end());

	const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) /
		static_cast<double>(samples.size());

	const std::size_t middle = samples.size() / 2;
	const double median = samples.size() % 2 == 0
		? (samples[middle - 1] + samples[middle]) / 2.0
		: samples[middle];

	return { samples.front(), mean, median, samples.back() };
}

void PrintSummary(const char* name, const Summary& summary, const char* unit,
	double operationsPerRun = 0.0, bool milliseconds = false)
{
	std::cout << std::left << std::setw(34) << name
		<< std::right << " min=" << std::fixed << std::setprecision(3) << summary.min << unit
		<< " mean=" << summary.mean << unit
		<< " median=" << summary.median << unit
		<< " max=" << summary.max << unit;

	if (operationsPerRun > 0.0)
	{
		const double throughput = milliseconds
			? operationsPerRun * 1'000.0 / summary.median
			: operationsPerRun * 1'000'000'000.0 / summary.median;
		std::cout << " throughput=" << std::setprecision(2) << throughput << " ops/sec";
	}

	std::cout << '\n';
}

BenchmarkOptions ParseOptions(int argc, char* argv[])
{
	BenchmarkOptions options;

	if (argc > 1)
		options.stlPath = argv[1];
	if (argc > 2)
		options.constructionIterations = std::stoull(argv[2]);
	if (argc > 3)
		options.lookupIterations = std::stoull(argv[3]);

	if (options.constructionIterations == 0)
		options.constructionIterations = 1;
	if (options.lookupIterations == 0)
		options.lookupIterations = 1;

	if (options.stlPath.empty())
	{
		options.stlPath = std::filesystem::path(__FILE__).parent_path().parent_path() /
			"GeometricHashTable" / "wavy_surface_100000.stl";
	}

	return options;
}

Triangle BuildTriangle(double x1, double y1, double z1,
	double x2, double y2, double z2,
	double x3, double y3, double z3)
{
	return Triangle{
		Point3{ x1, y1, z1 },
		Point3{ x2, y2, z2 },
		Point3{ x3, y3, z3 } };
}

struct BenchmarkSlotLayout
{
	std::uint8_t IsOccupied;
	alignas(Triangle) std::uint8_t TriangleStorage[sizeof(Triangle)];
};

void InitializeSlotFlagsSerial(std::vector<BenchmarkSlotLayout>& slots)
{
	for (auto& slot : slots)
		slot.IsOccupied = 0;
}

void InitializeSlotFlagsParallel(std::vector<BenchmarkSlotLayout>& slots)
{
	std::for_each(std::execution::par_unseq, slots.begin(), slots.end(),
		[](BenchmarkSlotLayout& slot) { slot.IsOccupied = 0; });
}

void InitializeSlotsWithBulkZero(std::vector<BenchmarkSlotLayout>& slots)
{
	std::memset(slots.data(), 0, slots.size() * sizeof(BenchmarkSlotLayout));
}

void RunInitializationStrategy(const char* name, const BenchmarkOptions& options,
	void (*initialize)(std::vector<BenchmarkSlotLayout>&))
{
	std::vector<BenchmarkSlotLayout> slots(TABLE_SIZE);
	initialize(slots);

	std::vector<double> samples;
	samples.reserve(options.constructionIterations);

	for (std::size_t iteration = 0; iteration < options.constructionIterations; ++iteration)
	{
		std::memset(slots.data(), 0xFF, slots.size() * sizeof(BenchmarkSlotLayout));

		const auto start = Clock::now();
		initialize(slots);
		const auto end = Clock::now();

		if (!std::all_of(slots.begin(), slots.end(),
			[](const BenchmarkSlotLayout& slot) { return slot.IsOccupied == 0; }))
		{
			throw std::runtime_error(std::string("Initialization benchmark failed: ") + name);
		}

		samples.push_back(
			std::chrono::duration_cast<Duration>(end - start).count() / 1'000'000.0);
	}

	PrintSummary(name, Summarize(samples), " ms");
}

void RunInitializationBenchmarks(const BenchmarkOptions& options)
{
	const double storageMiB = static_cast<double>(TABLE_SIZE * sizeof(BenchmarkSlotLayout)) /
		(1024.0 * 1024.0);
	std::cout << "\nSlot initialization strategies (" << TABLE_SIZE << " slots, "
		<< std::fixed << std::setprecision(1) << storageMiB
		<< " MiB; initialization only)\n";
	RunInitializationStrategy("init-slot-flags-serial", options, InitializeSlotFlagsSerial);
	RunInitializationStrategy("init-slot-flags-parallel", options, InitializeSlotFlagsParallel);
	RunInitializationStrategy("init-slots-bulk-zero", options, InitializeSlotsWithBulkZero);
}

int RunConstructionBenchmarks(const BenchmarkOptions& options)
{
	if (!std::filesystem::exists(options.stlPath))
	{
		std::cerr << "STL fixture not found: " << options.stlPath << '\n';
		return 1;
	}

	std::vector<double> samples;
	samples.reserve(options.constructionIterations);

	{
		HashTable warmup(TABLE_SIZE);
		warmup.Construct(options.stlPath.string());
	}

	for (std::size_t iteration = 0; iteration < options.constructionIterations; ++iteration)
	{
		HashTable table(TABLE_SIZE);
		const auto start = Clock::now();
		table.Construct(options.stlPath.string());
		const auto end = Clock::now();
		samples.push_back(std::chrono::duration_cast<Duration>(end - start).count() / 1'000'000.0);
	}

	const Summary summary = Summarize(samples);
	std::cout << "\nConstruction (100k-facet STL, table-size summary)\n";
	std::cout << "  fixture: " << options.stlPath << '\n';
	PrintSummary("construct-table-1m", summary, " ms", 100'000.0, true);

	{
		std::vector<double> reuseSamples;
		reuseSamples.reserve(options.constructionIterations);
		HashTable reusedTable(TABLE_SIZE);

		for (std::size_t iteration = 0; iteration < options.constructionIterations; ++iteration)
		{
			const auto start = Clock::now();
			reusedTable.Construct(options.stlPath.string());
			const auto end = Clock::now();
			reuseSamples.push_back(
				std::chrono::duration_cast<Duration>(end - start).count() / 1'000'000.0);
		}

		PrintSummary("construct-reuse-table-1m", Summarize(reuseSamples), " ms", 100'000.0, true);
	}

	for (const uint32_t tableSize : { 250'000u, 4'000'000u })
	{
		std::vector<double> scalingSamples;
		scalingSamples.reserve(options.constructionIterations);

		for (std::size_t iteration = 0; iteration < options.constructionIterations; ++iteration)
		{
			HashTable table(tableSize);
			const auto start = Clock::now();
			table.Construct(options.stlPath.string());
			const auto end = Clock::now();
			scalingSamples.push_back(
				std::chrono::duration_cast<Duration>(end - start).count() / 1'000'000.0);
		}

		const Summary scalingSummary = Summarize(scalingSamples);
		const std::string name = tableSize >= 1'000'000
			? "construct-table-" + std::to_string(tableSize / 1'000'000) + "m"
			: "construct-table-" + std::to_string(tableSize / 1'000) + "k";
		PrintSummary(name.c_str(), scalingSummary, " ms", 100'000.0, true);
	}

	return 0;
}

void RunLookupBenchmark(const char* name, const HashTable& table,
	const Triangle& triangle, std::size_t iterations,
	bool expectedResult)
{
	std::uint64_t checksum = 0;

	const auto start = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		checksum += table.Lookup(triangle) ? 1 : 0;
	const auto end = Clock::now();

	const double elapsedNanoseconds = std::chrono::duration_cast<Duration>(end - start).count();
	const double nsPerLookup = elapsedNanoseconds / static_cast<double>(iterations);

	const bool checksumMatches = expectedResult
		? checksum == iterations
		: checksum == 0;

	if (!checksumMatches)
		throw std::runtime_error(std::string("Lookup benchmark checksum mismatch: ") + name);

	std::cout << std::left << std::setw(34) << name
		<< std::right << " time=" << std::fixed << std::setprecision(3)
		<< nsPerLookup << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / nsPerLookup << " ops/sec\n";
}

std::vector<Triangle> LoadLookupTriangles(const std::filesystem::path& stlPath,
	std::size_t maximumCount)
{
	std::ifstream input(stlPath);
	if (!input)
		throw std::runtime_error("Unable to open STL fixture for randomized lookup benchmark.");

	std::vector<Triangle> triangles;
	triangles.reserve(std::min(maximumCount, std::size_t{ 16'384 }));
	std::array<Point3, 3> vertices;
	std::size_t vertexCount = 0;
	std::string line;

	while (triangles.size() < maximumCount && std::getline(input, line))
	{
		const std::size_t firstContent = line.find_first_not_of(" \t\r");
		if (firstContent == std::string::npos)
			continue;

		const std::string_view vertexLine(line.data() + firstContent, line.size() - firstContent);
		if (!vertexLine.starts_with("vertex"))
			continue;

		GetVertex(vertexLine, vertices[vertexCount]);
		if (++vertexCount == vertices.size())
		{
			triangles.emplace_back(vertices[0], vertices[1], vertices[2]);
			vertexCount = 0;
		}
	}

	if (triangles.empty())
		throw std::runtime_error("STL fixture contains no ASCII triangle vertices.");

	return triangles;
}

void RunRandomizedLookupBenchmark(const char* name, const HashTable& table,
	const std::vector<Triangle>& queries, const std::vector<std::uint8_t>& expectedResults,
	std::size_t iterations)
{
	if (queries.empty() || queries.size() != expectedResults.size())
		throw std::invalid_argument("Randomized lookup benchmark requires matching non-empty queries and expectations.");

	std::vector<std::size_t> order(queries.size());
	std::iota(order.begin(), order.end(), 0);
	std::mt19937 random(0x5EEDu);
	std::shuffle(order.begin(), order.end(), random);

	std::uint64_t expectedChecksum = 0;
	for (std::size_t index = 0; index < iterations; ++index)
		expectedChecksum += expectedResults[order[index % order.size()]];

	std::uint64_t checksum = 0;
	const auto start = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		checksum += table.Lookup(queries[order[index % order.size()]]) ? 1 : 0;
	const auto end = Clock::now();

	const double elapsedNanoseconds = std::chrono::duration_cast<Duration>(end - start).count();
	const double nsPerLookup = elapsedNanoseconds / static_cast<double>(iterations);
	if (checksum != expectedChecksum)
		throw std::runtime_error(std::string("Randomized lookup checksum mismatch: ") + name);

	std::cout << std::left << std::setw(34) << name
		<< std::right << " queries=" << queries.size()
		<< " time=" << std::fixed << std::setprecision(3) << nsPerLookup << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / nsPerLookup << " ops/sec\n";
}

int RunLookupBenchmarks(const BenchmarkOptions& options)
{
	HashTable table(TABLE_SIZE);
	table.Construct(options.stlPath.string());

	const Triangle known = BuildTriangle(
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.6661729,
		1.0, 1.0, 0.6646931);

	const Triangle reordered = BuildTriangle(
		1.0, 1.0, 0.6646931,
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.6661729);

	const Triangle missing = BuildTriangle(
		9.0, 9.0, 9.0,
		9.0, 9.0, 9.1,
		9.0, 9.1, 9.0);

	table.Lookup(known);

	std::cout << "\nLookup (" << options.lookupIterations << " iterations)\n";
	RunLookupBenchmark("lookup-known", table, known, options.lookupIterations, true);
	RunLookupBenchmark("lookup-known-reordered", table, reordered, options.lookupIterations, true);
	RunLookupBenchmark("lookup-missing", table, missing, options.lookupIterations, false);

	const Triangle nearTolerance = BuildTriangle(
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.6661729,
		1.0 + detail::EPSILON / 2.0, 1.0, 0.6646931);
	RunLookupBenchmark("lookup-near-tolerance", table, nearTolerance, options.lookupIterations,
		false);

	for (const uint32_t tableSize : { 250'000u, 1'000'000u, 4'000'000u })
	{
		HashTable sizedTable(tableSize);
		sizedTable.Construct(options.stlPath.string());
		const std::string knownName = "lookup-known-table-" +
			(tableSize >= 1'000'000
				? std::to_string(tableSize / 1'000'000) + "m"
				: std::to_string(tableSize / 1'000) + "k");
		RunLookupBenchmark(knownName.c_str(), sizedTable, known,
			options.lookupIterations, true);
	}

	HashTable collisionTable(1);
	collisionTable.Construct(options.stlPath.string());
	RunLookupBenchmark("lookup-one-bucket-collision", collisionTable, known,
		options.lookupIterations, true);

	std::uint64_t checksum = 0;
	const auto start = Clock::now();
	for (std::size_t index = 0; index < options.lookupIterations; ++index)
	{
		if (index % 3 == 0)
			checksum += table.Lookup(known) ? 1 : 0;
		else if (index % 3 == 1)
			checksum += table.Lookup(reordered) ? 1 : 0;
		else
			checksum += table.Lookup(missing) ? 1 : 0;
	}
	const auto end = Clock::now();

	const double elapsedNanoseconds = std::chrono::duration_cast<Duration>(end - start).count();
	const double nsPerLookup = elapsedNanoseconds / static_cast<double>(options.lookupIterations);
	const std::uint64_t expectedKnownAndReordered =
		options.lookupIterations - options.lookupIterations / 3;

	std::cout << std::left << std::setw(34) << "lookup-mixed-2hit-1miss"
		<< std::right << " time=" << std::fixed << std::setprecision(3)
		<< nsPerLookup << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / nsPerLookup << " ops/sec\n";

	if (checksum != expectedKnownAndReordered)
		throw std::runtime_error("Mixed lookup benchmark checksum mismatch.");

	const auto lookupTriangles = LoadLookupTriangles(options.stlPath, 16'384);
	const auto allTriangles = LoadLookupTriangles(options.stlPath,
		std::numeric_limits<std::size_t>::max());
	std::unordered_map<std::uint32_t, Triangle> referenceSlots;
	for (const Triangle& sourceTriangle : allTriangles)
		referenceSlots.try_emplace(hashTriangle(sourceTriangle[0], sourceTriangle[1],
				 sourceTriangle[2], TABLE_SIZE, detail::EPSILON, true), sourceTriangle);

	for (const std::size_t requestedSize : { 64u, 1'024u, 16'384u })
	{
		const std::size_t queryCount = std::min(requestedSize, lookupTriangles.size());
		const std::vector<Triangle> queries(
			lookupTriangles.begin(), lookupTriangles.begin() + queryCount);
		std::vector<std::uint8_t> expectedResults;
		expectedResults.reserve(queryCount);
		for (const Triangle& query : queries)
		{
			const auto found = referenceSlots.find(hashTriangle(query[0], query[1],
				query[2], TABLE_SIZE, detail::EPSILON, true));
			expectedResults.push_back(found != referenceSlots.end() && found->second == query);
		}

		const std::string name = "lookup-random-" + std::to_string(queryCount);
		RunRandomizedLookupBenchmark(name.c_str(), table, queries, expectedResults,
			options.lookupIterations);
	}

	return 0;
}

void RunHashBenchmarks(std::size_t iterations)
{
	const Point3 a{ 1.25, -2.5, 3.75 };
	const Point3 b{ -4.0, 5.0, 6.0 };
	const Point3 c{ 7.0, 8.0, -9.0 };

	std::cout << "\nHash microbenchmark (" << iterations << " iterations)\n";

	std::uint64_t orderIndependentChecksum = 0;
	const auto orderIndependentStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		orderIndependentChecksum += hashTriangle(a, b, c, TABLE_SIZE, 1e-6, true);
	const auto orderIndependentEnd = Clock::now();
	const double orderIndependentNs = std::chrono::duration_cast<Duration>(
		orderIndependentEnd - orderIndependentStart).count() / static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "hashTriangle-order-independent"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << orderIndependentNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / orderIndependentNs << " hashes/sec"
		<< " checksum=" << orderIndependentChecksum << '\n';

	std::uint64_t windingSensitiveChecksum = 0;
	const auto windingSensitiveStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		windingSensitiveChecksum += hashTriangle(a, b, c, TABLE_SIZE, 1e-6, false);
	const auto windingSensitiveEnd = Clock::now();
	const double windingSensitiveNs = std::chrono::duration_cast<Duration>(
		windingSensitiveEnd - windingSensitiveStart).count() / static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "hashTriangle-winding-sensitive"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << windingSensitiveNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / windingSensitiveNs << " hashes/sec"
		<< " checksum=" << windingSensitiveChecksum << '\n';

	std::uint64_t pointChecksum = 0;
	const auto pointStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		pointChecksum += hashPoint(a, 1e-6);
	const auto pointEnd = Clock::now();
	const double pointNs = std::chrono::duration_cast<Duration>(
		pointEnd - pointStart).count() / static_cast<double>(iterations);

	std::uint64_t quantizeChecksum = 0;
	const auto quantizeStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		quantizeChecksum += static_cast<std::uint64_t>(quantize(a.x + static_cast<double>(index % 1'000) * 1e-6, 1e-6));
	const auto quantizeEnd = Clock::now();
	const double quantizeNs = std::chrono::duration_cast<Duration>(
		quantizeEnd - quantizeStart).count() / static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "quantize"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << quantizeNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / quantizeNs << " quantizes/sec"
		<< " checksum=" << quantizeChecksum << '\n';

	std::cout << std::left << std::setw(34) << "hashPoint"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << pointNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / pointNs << " hashes/sec"
		<< " checksum=" << pointChecksum << '\n';

	std::uint64_t permutationChecksum = 0;
	const auto permutationStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		permutationChecksum += hashTriangle(a, b, c, TABLE_SIZE, 1e-6, true);
		permutationChecksum += hashTriangle(b, c, a, TABLE_SIZE, 1e-6, true);
		permutationChecksum += hashTriangle(c, a, b, TABLE_SIZE, 1e-6, true);
	}
	const auto permutationEnd = Clock::now();
	const double permutationNs = std::chrono::duration_cast<Duration>(
		permutationEnd - permutationStart).count() /
		(static_cast<double>(iterations) * 3.0);

	std::cout << std::left << std::setw(34) << "hashTriangle-three-permutations"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << permutationNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / permutationNs << " hashes/sec"
		<< " checksum=" << permutationChecksum << '\n';

	std::uint64_t mixerChecksum = 0;
	const auto mixerStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
		mixerChecksum += mix64(static_cast<std::uint64_t>(index));
	const auto mixerEnd = Clock::now();
	const double mixerNs = std::chrono::duration_cast<Duration>(
		mixerEnd - mixerStart).count() / static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "mix64"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << mixerNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / mixerNs << " mixes/sec"
		<< " checksum=" << mixerChecksum << '\n';

	Point3 parsedPoint;
	const std::string_view vertexLine("vertex 1.25 -2.5 3.75");
	std::uint64_t parserChecksum = 0;
	const auto parserStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		GetVertex(vertexLine, parsedPoint);
		parserChecksum += static_cast<std::uint64_t>(parsedPoint.x * 1'000.0)
			+ static_cast<std::uint64_t>(parsedPoint.y * 1'000.0)
			+ static_cast<std::uint64_t>(parsedPoint.z * 1'000.0);
	}
	const auto parserEnd = Clock::now();
	const double parserNs = std::chrono::duration_cast<Duration>(
		parserEnd - parserStart).count() / static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "GetVertex-reference"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << parserNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / parserNs << " parses/sec"
		<< " checksum=" << parserChecksum << '\n';
}

void RunTriangleHashDistributionBenchmark(std::size_t iterations)
{
	constexpr std::uint32_t distributionBucketCount = 4096;
	std::vector<std::uint32_t> occupancy(distributionBucketCount, 0);

	std::uint64_t checksum = 0;
	const auto start = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		const double coordinate = static_cast<double>(index % 1'000'000) * 1e-6;
		const Point3 a{ coordinate, coordinate * 0.25, -coordinate * 0.5 };
		const Point3 b{ coordinate + 1.0, -coordinate * 0.75, coordinate * 1.25 };
		const Point3 c{ -coordinate, coordinate * 0.5, coordinate + 2.0 };
		const std::uint32_t bucket = hashTriangle(a, b, c, distributionBucketCount) % distributionBucketCount;
		++occupancy[bucket];
		checksum += bucket;
	}
	const auto end = Clock::now();

	std::uint32_t emptyBuckets = 0;
	std::uint32_t maximumOccupancy = 0;
	for (const std::uint32_t count : occupancy)
	{
		emptyBuckets += count == 0 ? 1u : 0u;
		maximumOccupancy = std::max(maximumOccupancy, count);
	}

	const double elapsedNanoseconds = std::chrono::duration_cast<Duration>(end - start).count();
	const double nsPerHash = elapsedNanoseconds / static_cast<double>(iterations);

	std::cout << "\nTriangle hash distribution (" << iterations << " 4096-bucket samples)\n";
	std::cout << std::left << std::setw(34) << "hashTriangle-distribution"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << nsPerHash << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / nsPerHash << " hashes/sec"
		<< " empty=" << emptyBuckets
		<< " max-occupancy=" << maximumOccupancy
		<< " checksum=" << checksum << '\n';
}

void RunHashDistributionBenchmark(std::size_t iterations)
{
	constexpr std::uint32_t distributionBucketCount = 4096;
	std::vector<std::uint32_t> occupancy(distributionBucketCount, 0);

	std::uint64_t checksum = 0;
	const auto start = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		const double coordinate = static_cast<double>(index % 1'000'000) * 1e-6;
		const Point3 point{
			coordinate,
			-coordinate * 0.75,
			coordinate * 1.25 };
		const std::uint32_t bucket = hashPoint(point, 1e-6) % distributionBucketCount;
		++occupancy[bucket];
		checksum += bucket;
	}
	const auto end = Clock::now();

	std::uint32_t emptyBuckets = 0;
	std::uint32_t maximumOccupancy = 0;
	for (const std::uint32_t count : occupancy)
	{
		emptyBuckets += count == 0 ? 1u : 0u;
		maximumOccupancy = std::max(maximumOccupancy, count);
	}

	const double elapsedNanoseconds = std::chrono::duration_cast<Duration>(end - start).count();
	const double nsPerHash = elapsedNanoseconds / static_cast<double>(iterations);

	std::cout << "\nHash distribution (" << iterations << " 4096-bucket samples)\n";
	std::cout << std::left << std::setw(34) << "hashPoint-distribution"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << nsPerHash << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / nsPerHash << " hashes/sec"
		<< " empty=" << emptyBuckets
		<< " max-occupancy=" << maximumOccupancy
		<< " checksum=" << checksum << '\n';
}

void RunToleranceAndNormalizationBenchmarks(std::size_t iterations)
{
	std::uint64_t toleranceChecksum = 0;
	const auto toleranceStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		const double coordinate = static_cast<double>(index % 10'000) * 1e-6;
		const Point3 point{ coordinate, -coordinate * 2.0, coordinate * 3.0 };
		toleranceChecksum += hashPoint(point, 1e-6);
		toleranceChecksum += hashPoint(point, 1e-3);
	}
	const auto toleranceEnd = Clock::now();
	const double toleranceNs = std::chrono::duration_cast<Duration>(
		toleranceEnd - toleranceStart).count() /
		(static_cast<double>(iterations) * 2.0);

	std::cout << "\nTolerance and normalization microbenchmarks (" << iterations << " iterations)\n";
	std::cout << std::left << std::setw(34) << "hashPoint-two-tolerances"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << toleranceNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / toleranceNs << " hashes/sec"
		<< " checksum=" << toleranceChecksum << '\n';

	std::uint64_t normalizationChecksum = 0;
	const auto normalizationStart = Clock::now();
	for (std::size_t index = 0; index < iterations; ++index)
	{
		Triangle triangle{
			Point3{ static_cast<double>(index % 17), 4.0, -2.0 },
			Point3{ -1.0, static_cast<double>(index % 11), 8.0 },
			Point3{ 2.0, -3.0, static_cast<double>(index % 19) }
		};
		normalizationChecksum += mix64(static_cast<std::uint64_t>(triangle[0].x)
			^ static_cast<std::uint64_t>(triangle[2].z));
	}
	const auto normalizationEnd = Clock::now();
	const double normalizationNs = std::chrono::duration_cast<Duration>(
		normalizationEnd - normalizationStart).count() /
		static_cast<double>(iterations);

	std::cout << std::left << std::setw(34) << "triangle-constructor-normalize"
		<< std::right << std::fixed << std::setprecision(3)
		<< " time=" << normalizationNs << " ns/op"
		<< " throughput=" << std::setprecision(2)
		<< 1'000'000'000.0 / normalizationNs << " triangles/sec"
		<< " checksum=" << normalizationChecksum << '\n';
}

void PrintUsage(const char* executable)
{
	std::cout
		<< "Usage: " << executable
		<< " [stl-file] [construction-runs] [lookup-iterations]\n"
		<< "Defaults: wavy_surface_100000.stl, 5, 1000000\n";
}

} // namespace

int main(int argc, char* argv[])
{
	if (argc > 1 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h"))
	{
		PrintUsage(argv[0]);
		return EXIT_SUCCESS;
	}

	try
	{
		const BenchmarkOptions options = ParseOptions(argc, argv);

		std::cout << "Geometric Hash Table Benchmarks\n"
			<< "===============================\n"
			<< "Configuration: Release recommended\n"
			<< "Table size: " << TABLE_SIZE << " buckets\n"
			<< "Construction runs: " << options.constructionIterations << '\n';

		if (RunConstructionBenchmarks(options) != 0)
			return EXIT_FAILURE;
		RunInitializationBenchmarks(options);
		if (RunLookupBenchmarks(options) != 0)
			return EXIT_FAILURE;

		RunHashBenchmarks(options.lookupIterations);
		RunHashDistributionBenchmark(options.lookupIterations);
		RunTriangleHashDistributionBenchmark(options.lookupIterations);
		RunToleranceAndNormalizationBenchmarks(options.lookupIterations);
		return EXIT_SUCCESS;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Benchmark failed: " << exception.what() << '\n';
		return EXIT_FAILURE;
	}
}
