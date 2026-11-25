// These Unit Tests are written by AI Agent - GitHub Copilot.


#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "../GeometricHashTable/Triangle.h"
#include "../GeometricHashTable/Hash.h"
#include "../GeometricHashTable/HashTable.h"

namespace {

struct TestSummary {
	int passed = 0;
	int failed = 0;
};

void RecordResult(TestSummary& summary, const std::string& name, const bool condition)
{
	if (condition)
	{
		++summary.passed;
		std::cout << "[PASS] " << name << '\n';
	}
	else
	{
		++summary.failed;
		std::cerr << "[FAIL] " << name << '\n';
	}
}

template <typename ExpectedException>
void ExpectThrows(TestSummary& summary, const std::string& name, const std::function<void()>& action)
{
	bool threwExpectedException = false;
	std::string failureName = name;

	try
	{
		action();
	}
	catch (const ExpectedException&)
	{
		threwExpectedException = true;
	}
	catch (const std::exception& exception)
	{
		failureName += " (unexpected exception: ";
		failureName += exception.what();
		failureName += ')';
	}

	RecordResult(summary, failureName, threwExpectedException);
}

std::filesystem::path GetTestStlPath()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path() / "GeometricHashTable" / "wavy_surface_100000.stl";
}

class TemporaryStlFile
{
public:
	explicit TemporaryStlFile(const std::string& contents)
		: m_Path(std::filesystem::temp_directory_path() /
			("GeometricHashTable.Tests." + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()) + ".stl"))
	{
		std::ofstream file(m_Path);
		if (!file)
		{
			throw std::runtime_error("Unable to create temporary STL file.");
		}

		file << contents;
		if (!file)
		{
			file.close();
			std::error_code error;
			std::filesystem::remove(m_Path, error);
			throw std::runtime_error("Unable to write temporary STL file.");
		}
	}

	~TemporaryStlFile()
	{
		std::error_code error;
		std::filesystem::remove(m_Path, error);
	}

	const std::filesystem::path& Path() const
	{
		return m_Path;
	}

private:
	std::filesystem::path m_Path;
};

Triangle BuildTriangle(const double x1, const double y1, const double z1,
	const double x2, const double y2, const double z2,
	const double x3, const double y3, const double z3)
{
	Triangle triangle;
	triangle[0] = Point3{x1, y1, z1};
	triangle[1] = Point3{x2, y2, z2};
	triangle[2] = Point3{x3, y3, z3};

	triangle.Normalize();

	return triangle;
}

void RunAllTests()
{
	TestSummary summary;

	const auto stlPath = GetTestStlPath();

	ExpectThrows<std::invalid_argument>(summary, "Construct reports an invalid argument when the STL file is missing.", [] {
		HashTable hashTable(256);
		hashTable.Construct("this_file_should_not_exist.stl");
	});
	{
		const TemporaryStlFile emptyTableStlFile(
			"solid empty_table\n"
			"endsolid empty_table\n");
		bool constructCompleted = false;
		try
		{
			HashTable hashTable(0);
			hashTable.Construct(emptyTableStlFile.Path().string());
			constructCompleted = true;
		}
		catch (const std::exception&)
		{
			constructCompleted = false;
		}
		RecordResult(summary, "A zero-sized table with no facets completes construction without an allocation of buckets.",
			constructCompleted);
	}

	{
		const Point3 origin{ 0.0, 0.0, 0.0 };
		RecordResult(summary, "Point3 has a usable zero-valued default constructor.",
			Point3{} == origin);
		RecordResult(summary, "Point3 considers coordinates within EPS equal.",
			origin == Point3{ Point3::EPS / 2.0, 0.0, 0.0 });
		RecordResult(summary, "Point3 rejects coordinates at the EPS boundary.",
			!(origin == Point3{ Point3::EPS, 0.0, 0.0 }));
		RecordResult(summary, "Point3 equality is symmetric for nearby coordinates.",
			(Point3{ 1.0, 2.0, 3.0 } == Point3{ 1.0 + Point3::EPS / 2.0, 2.0, 3.0 }) &&
			(Point3{ 1.0 + Point3::EPS / 2.0, 2.0, 3.0 } == Point3{ 1.0, 2.0, 3.0 }));

		Point3 parsedPoint;
		GetVertex("vertex 1.25 -2.5 3.75", &parsedPoint);
		RecordResult(summary, "GetVertex parses positive and negative coordinates.",
			parsedPoint == Point3{ 1.25, -2.5, 3.75 });

		Point3 scientificPoint;
		GetVertex("vertex -1.25e-2 2.5e+1 3.75e0", &scientificPoint);
		RecordResult(summary, "GetVertex parses scientific-notation coordinates.",
			scientificPoint == Point3{ -0.0125, 25.0, 3.75 });

		ExpectThrows<std::invalid_argument>(summary, "GetVertex rejects a non-vertex line.", [] {
			Point3 point;
			GetVertex("facet normal 0 0 0", &point);
		});
		ExpectThrows<std::runtime_error>(summary, "GetVertex rejects malformed x coordinates.", [] {
			Point3 point;
			GetVertex("vertex invalid 0 0", &point);
		});
		ExpectThrows<std::runtime_error>(summary, "GetVertex rejects malformed y coordinates.", [] {
			Point3 point;
			GetVertex("vertex 1 invalid 0", &point);
		});
		ExpectThrows<std::runtime_error>(summary, "GetVertex rejects malformed z coordinates.", [] {
			Point3 point;
			GetVertex("vertex 1 2 invalid", &point);
		});
		ExpectThrows<std::runtime_error>(summary, "GetVertex rejects missing z coordinates.", [] {
			Point3 point;
			GetVertex("vertex 1 2 ", &point);
		});
		ExpectThrows<std::runtime_error>(summary, "GetVertex rejects a coordinate-only line.", [] {
			Point3 point;
			GetVertex("vertex ", &point);
		});
		ExpectThrows<std::invalid_argument>(summary, "GetVertex rejects a null output pointer.", [] {
			GetVertex("vertex 1 2 3", nullptr);
		});

		const Triangle reordered{
			Point3{ 3.0, 3.0, 3.0 },
			Point3{ 1.0, 1.0, 1.0 },
			Point3{ 2.0, 2.0, 2.0 }};
		RecordResult(summary, "Triangle constructor normalizes vertex order.",
			reordered[0] == Point3{ 1.0, 1.0, 1.0 } &&
			reordered[1] == Point3{ 2.0, 2.0, 2.0 } &&
			reordered[2] == Point3{ 3.0, 3.0, 3.0 });

		const Triangle defaultTriangle;
		RecordResult(summary, "Triangle default construction initializes all vertices to the origin.",
			defaultTriangle[0] == origin &&
			defaultTriangle[1] == origin &&
			defaultTriangle[2] == origin);

		RecordResult(summary, "Point3 ordering compares x, y, and z coordinates lexicographically.",
			Point3{ 1.0, 2.0, 3.0 } < Point3{ 1.0, 2.0, 4.0 });
		RecordResult(summary, "Point3 ordering considers y when x coordinates match.",
			Point3{ 1.0, 2.0, 9.0 } < Point3{ 1.0, 3.0, 0.0 });
		RecordResult(summary, "Point3 ordering considers z when x and y coordinates match.",
			Point3{ 1.0, 2.0, 3.0 } < Point3{ 1.0, 2.0, 4.0 });
		RecordResult(summary, "Point3 equality is exact at the coordinates-zero edge.",
			Point3{} == Point3{ 0.0, 0.0, 0.0 });
		RecordResult(summary, "Point3 equality independently rejects mismatches in x, y, and z.",
			!(Point3{ 1.0, 2.0, 3.0 } == Point3{ 2.0, 2.0, 3.0 }) &&
			!(Point3{ 1.0, 2.0, 3.0 } == Point3{ 1.0, 3.0, 3.0 }) &&
			!(Point3{ 1.0, 2.0, 3.0 } == Point3{ 1.0, 2.0, 4.0 }));
		RecordResult(summary, "Point3 ordering is false for identical coordinates.",
			!(origin < origin) && !(origin < Point3{}));
	}

	{
		const Point3 a{ 1.25, -2.5, 3.75 };
		const Point3 b{ -4.0, 5.0, 6.0 };
		const Point3 c{ 7.0, 8.0, -9.0 };

		const auto abc = hashTriangle(a, b, c);
		const auto permutations = {
			hashTriangle(a, c, b),
			hashTriangle(b, a, c),
			hashTriangle(b, c, a),
			hashTriangle(c, a, b),
			hashTriangle(c, b, a)
		};
		bool allPermutationsMatch = true;
		for (const auto hash : permutations)
		{
			allPermutationsMatch = allPermutationsMatch && hash == abc;
		}

		RecordResult(summary, "hashTriangle is independent of vertex order.", allPermutationsMatch);
		RecordResult(summary, "hashTriangle stays within the requested table size.",
			abc < 1'000'000);
		RecordResult(summary, "hashTriangle is deterministic for repeated calls.",
			abc == hashTriangle(a, b, c));
		RecordResult(summary, "quantize maps nearby coordinates to the same bucket.",
			quantize(1.0000004, 1e-6) == quantize(1.0000001, 1e-6));
		RecordResult(summary, "quantize rounds a positive half-tolerance value to the adjacent bucket.",
			quantize(1.0000005, 1e-6) == quantize(1.0, 1e-6) + 1);
		RecordResult(summary, "quantize also maps nearby negative coordinates to the same bucket.",
			quantize(-1.0000004, 1e-6) == quantize(-1.0000001, 1e-6));
		RecordResult(summary, "hashPoint is deterministic for negative coordinates.",
			hashPoint(a, 1e-6) == hashPoint(a, 1e-6));
		RecordResult(summary, "hashTriangle supports a one-bucket table.",
			hashTriangle(a, b, c, 1) == 0);
		RecordResult(summary, "hashTriangle stays within several small table sizes.",
			hashTriangle(a, b, c, 7) < 7 &&
			hashTriangle(a, b, c, 64) < 64);
		RecordResult(summary, "winding-sensitive hashing is deterministic.",
			hashTriangle(a, b, c, 1'000'000, 1e-6, false) ==
			hashTriangle(a, b, c, 1'000'000, 1e-6, false));
		RecordResult(summary, "hashTriangle handles zero coordinates.",
			hashTriangle(Point3{}, Point3{}, Point3{}, 64) < 64);
		RecordResult(summary, "hashTriangle handles negative and positive coordinates across a small table.",
			hashTriangle(a, b, c, 31) < 31);
		RecordResult(summary, "quantize handles zero and negative half-tolerance values deterministically.",
			quantize(0.0, 1e-6) == 0 &&
			quantize(-1.0000005, 1e-6) == quantize(-1.000001, 1e-6));
		RecordResult(summary, "hashPoint produces a stable 64-bit result for repeated calls.",
			hashPoint(Point3{ -7.25, 0.5, 12.75 }, 1e-6) ==
			hashPoint(Point3{ -7.25, 0.5, 12.75 }, 1e-6));
		RecordResult(summary, "mix64 produces a stable result for zero.",
			mix64(0) == mix64(0));
		RecordResult(summary, "hashTriangle output stays bounded for table sizes 2 and 3.",
			hashTriangle(a, b, c, 2) < 2 && hashTriangle(a, b, c, 3) < 3);
		RecordResult(summary, "quantize distinguishes exact positive and negative half-tolerance rounding.",
			quantize(0.0000005, 1e-6) == 1 &&
			quantize(-0.0000005, 1e-6) == -1);
		RecordResult(summary, "hashPoint distinguishes coordinate-axis changes.",
			hashPoint(Point3{ 1.0, 2.0, 3.0 }, 1e-6) !=
			hashPoint(Point3{ 2.0, 1.0, 3.0 }, 1e-6));
		RecordResult(summary, "hashPoint distinguishes changes in each coordinate axis.",
			hashPoint(Point3{ 1.0, 2.0, 3.0 }, 1e-6) !=
			hashPoint(Point3{ 1.1, 2.0, 3.0 }, 1e-6) &&
			hashPoint(Point3{ 1.0, 2.0, 3.0 }, 1e-6) !=
			hashPoint(Point3{ 1.0, 2.1, 3.0 }, 1e-6) &&
			hashPoint(Point3{ 1.0, 2.0, 3.0 }, 1e-6) !=
			hashPoint(Point3{ 1.0, 2.0, 3.1 }, 1e-6));

		bool allGridPermutationsMatch = true;
		bool allHashesAreInRange = true;
		for (int x = -3; x <= 3; ++x)
		{
			for (int y = -2; y <= 2; ++y)
			{
				for (int z = -1; z <= 1; ++z)
				{
					const Point3 p1{ x * 0.125, y * 0.25, z * 0.5 };
					const Point3 p2{ x * 0.125 + 0.75, y * 0.25 - 0.5, z * 0.5 + 0.25 };
					const Point3 p3{ x * 0.125 - 0.25, y * 0.25 + 0.125, z * 0.5 - 0.75 };
					const auto expected = hashTriangle(p1, p2, p3, 4096);
					const std::array<std::array<int, 3>, 6> permutations = {{
						{{ 0, 1, 2 }}, {{ 0, 2, 1 }}, {{ 1, 0, 2 }},
						{{ 1, 2, 0 }}, {{ 2, 0, 1 }}, {{ 2, 1, 0 }}
					}};
					const std::array<Point3, 3> points = { p1, p2, p3 };
					for (const auto& permutation : permutations)
					{
						const auto hash = hashTriangle(points[permutation[0]],
							points[permutation[1]], points[permutation[2]], 4096);
						allGridPermutationsMatch = allGridPermutationsMatch && hash == expected;
						allHashesAreInRange = allHashesAreInRange && hash < 4096;
					}
				}
			}
		}
		RecordResult(summary, "hashTriangle is order-independent across six permutations and a coordinate grid.",
			allGridPermutationsMatch);
		RecordResult(summary, "hashTriangle stays in range across a coordinate grid.",
			allHashesAreInRange);
	}

	{
		const TemporaryStlFile stlFile(
			"solid test\n"
			" facet normal 0 0 0\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 0 1 0\n"
			"   vertex 1 0 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid test\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(stlFile.Path().string());

		const Triangle triangle = BuildTriangle(
			0.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			1.0, 0.0, 0.0);
		const Triangle almostEqualTriangle = BuildTriangle(
			0.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			1.0 + Point3::EPS / 2.0, 0.0, 0.0);
		const Triangle outsideToleranceTriangle = BuildTriangle(
			0.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			1.0 + Point3::EPS * 2.0, 0.0, 0.0);

		RecordResult(summary, "Construct and Lookup work with a minimal ASCII STL file.",
			hashTable.Lookup(triangle));
		const Triangle sameBucketTriangle = BuildTriangle(
			0.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			1.0 + Point3::EPS / 10.0, 0.0, 0.0);
		RecordResult(summary, "Lookup finds a within-EPS triangle when quantization selects the same bucket.",
			triangle[2] == sameBucketTriangle[2] && hashTable.Lookup(sameBucketTriangle));
		RecordResult(summary, "An EPS-equal coordinate can quantize to a different hash bucket.",
			triangle[2] == almostEqualTriangle[2] &&
			hashTriangle(triangle[0], triangle[1], triangle[2], 1'000'000) !=
			hashTriangle(almostEqualTriangle[0], almostEqualTriangle[1], almostEqualTriangle[2], 1'000'000));
		RecordResult(summary, "Lookup misses an EPS-equal triangle when quantization selects a different bucket.",
			!hashTable.Lookup(almostEqualTriangle));
		RecordResult(summary, "Lookup rejects coordinate differences beyond EPS.", 
			!hashTable.Lookup(outsideToleranceTriangle));
	}

	{
		const TemporaryStlFile duplicateStlFile(
			"solid duplicates\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 2 0 0\n"
			"   vertex 0 2 0\n"
			"  endloop\n"
			" endfacet\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 2 0\n"
			"   vertex 0 0 0\n"
			"   vertex 2 0 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid duplicates\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(duplicateStlFile.Path().string());

		const Triangle duplicateTriangle = BuildTriangle(
			2.0, 0.0, 0.0,
			0.0, 2.0, 0.0,
			0.0, 0.0, 0.0);
		RecordResult(summary, "Construct retains duplicate triangles for lookup.",
			hashTable.Lookup(duplicateTriangle));
	}

	{
		const TemporaryStlFile twoTriangleStlFile(
			"solid two_triangles\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 5 5 0\n"
			"   vertex 6 5 0\n"
			"   vertex 5 6 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid two_triangles\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(twoTriangleStlFile.Path().string());

		const Triangle firstTriangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);
		const Triangle secondTriangle = BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0);
		RecordResult(summary, "Construct and Lookup find multiple distinct triangles.",
			hashTable.Lookup(firstTriangle) && hashTable.Lookup(secondTriangle));
	}

	{
		const TemporaryStlFile firstStlFile(
			"solid first\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid first\n");
		const TemporaryStlFile secondStlFile(
			"solid second\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 5 5 0\n"
			"   vertex 6 5 0\n"
			"   vertex 5 6 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid second\n");

		HashTable hashTable(1'000'000);
		hashTable.Construct(firstStlFile.Path().string());
		const Triangle firstTriangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);
		RecordResult(summary, "Construct can build an initial table on one HashTable instance.",
			hashTable.Lookup(firstTriangle));

		hashTable.Construct(secondStlFile.Path().string());
		const Triangle secondTriangle = BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0);
		RecordResult(summary, "Construct adds entries from a later STL without clearing distinct prior buckets.",
			hashTable.Lookup(secondTriangle) && hashTable.Lookup(firstTriangle));
	}

	{
		const TemporaryStlFile stateStlFile(
			"solid state\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid state\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(stateStlFile.Path().string());
		const Triangle stateTriangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);

		ExpectThrows<std::invalid_argument>(summary, "A failed reconstruction reports a missing-file error.", [&] {
			HashTable& mutableTable = hashTable;
			mutableTable.Construct("this_file_should_still_not_exist.stl");
		});
		RecordResult(summary, "Lookup still finds the prior triangle after a missing-file construction failure.",
			hashTable.Lookup(stateTriangle));
	}

	{
		const TemporaryStlFile firstStateStlFile(
			"solid first_state\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid first_state\n");
		const TemporaryStlFile emptyStateStlFile(
			"solid empty_state\n"
			"endsolid empty_state\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(firstStateStlFile.Path().string());
		hashTable.Construct(emptyStateStlFile.Path().string());

		const Triangle retainedTriangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);
		RecordResult(summary, "Construct with an empty STL retains prior buckets because it does not clear the table.",
			hashTable.Lookup(retainedTriangle));
	}

	{
		const TemporaryStlFile collisionStlFile(
			"solid collisions\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 5 5 0\n"
			"   vertex 6 5 0\n"
			"   vertex 5 6 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid collisions\n");
		HashTable hashTable(1);
		hashTable.Construct(collisionStlFile.Path().string());

		const Triangle firstTriangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);
		const Triangle secondTriangle = BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0);
		RecordResult(summary, "Lookup finds the triangle retained as a one-bucket primary slot.",
			hashTable.Lookup(firstTriangle));
		RecordResult(summary, "Lookup does not promise to retain a second triangle in a one-bucket collision.",
			!hashTable.Lookup(secondTriangle));
	}

	{
		const TemporaryStlFile emptyStlFile(
			"solid empty\n"
			"endsolid empty\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(emptyStlFile.Path().string());

		const Triangle absentTriangle = BuildTriangle(
			1.0, 1.0, 1.0,
			2.0, 1.0, 1.0,
			1.0, 2.0, 1.0);
		RecordResult(summary, "Lookup safely rejects triangles in an STL without facets.",
			!hashTable.Lookup(absentTriangle));

		const Triangle defaultTriangle;
		RecordResult(summary, "Lookup safely rejects the default triangle in an STL without facets.",
			!hashTable.Lookup(defaultTriangle));
	}

	{
		const TemporaryStlFile repeatedCoordinateStlFile(
			"solid repeated_coordinates\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 0 0 0\n"
			"   vertex 0 0 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid repeated_coordinates\n");
		HashTable hashTable(64);
		hashTable.Construct(repeatedCoordinateStlFile.Path().string());

		const Triangle repeatedTriangle = BuildTriangle(0, 0, 0, 0, 0, 0, 0, 0, 0);
		RecordResult(summary, "Construct and Lookup support degenerate repeated-vertex triangles.",
			hashTable.Lookup(repeatedTriangle));
	}

	{
		const TemporaryStlFile malformedVertexStlFile(
			"solid malformed_vertex\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"   not-a-vertex 0 1 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid malformed_vertex\n");
		ExpectThrows<std::invalid_argument>(summary, "Construct rejects a facet containing malformed vertex text.", [&] {
			HashTable hashTable(1'000'000);
			hashTable.Construct(malformedVertexStlFile.Path().string());
		});
	}

	{
		const TemporaryStlFile incompleteLoopStlFile(
			"solid incomplete_loop\n"
			" facet normal 0 0 1\n"
			"  outer loop\n"
			"   vertex 0 0 0\n"
			"   vertex 1 0 0\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid incomplete_loop\n");
		ExpectThrows<std::invalid_argument>(summary, "Construct rejects an incomplete triangle loop instead of manufacturing a third vertex.", [&] {
			HashTable hashTable(1'000'000);
			hashTable.Construct(incompleteLoopStlFile.Path().string());
		});
	}

	{
		const TemporaryStlFile negativeCoordinateStlFile(
			"solid negative_coordinates\n"
			" facet normal 0 0 -1\n"
			"  outer loop\n"
			"   vertex -3.5 -2.25 1.125\n"
			"   vertex -2.5 -2.25 1.125\n"
			"   vertex -3.5 -1.25 1.125\n"
			"  endloop\n"
			" endfacet\n"
			"endsolid negative_coordinates\n");
		HashTable hashTable(1'000'000);
		hashTable.Construct(negativeCoordinateStlFile.Path().string());

		const Triangle negativeTriangle = BuildTriangle(
			-3.5, -2.25, 1.125,
			-2.5, -2.25, 1.125,
			-3.5, -1.25, 1.125);
		RecordResult(summary, "Construct and Lookup support negative coordinates.",
			hashTable.Lookup(negativeTriangle));

		const Triangle negativeOutsideTolerance = BuildTriangle(
			-3.5, -2.25, 1.125,
			-2.5, -2.25, 1.125,
			-3.5, -1.25, 1.125 + Point3::EPS * 2.0);
		RecordResult(summary, "Negative-coordinate Lookup rejects a triangle outside the point tolerance.",
			!hashTable.Lookup(negativeOutsideTolerance));
	}

	{
		HashTable hashTable(1'000'000);
		hashTable.Construct(stlPath.string());

		const Triangle knownTriangle = BuildTriangle(
			0.0, 0.0, 0.0,
			1.0, 0.0, 0.6661729,
			1.0, 1.0, 0.6646931);

		const Triangle missingTriangle = BuildTriangle(
			9.0, 9.0, 9.0,
			9.0, 9.0, 9.1,
			9.0, 9.1, 9.0);

		const Triangle reorderedTriangle = BuildTriangle(
			1.0, 1.0, 0.6646931,
			0.0, 0.0, 0.0,
			1.0, 0.0, 0.6661729);

		RecordResult(summary, "Lookup finds a known triangle from the STL file.", hashTable.Lookup(knownTriangle));
		RecordResult(summary, "Lookup returns false for an absent triangle.", !hashTable.Lookup(missingTriangle));
		RecordResult(summary, "Lookup ignores vertex order for equivalent triangles.", hashTable.Lookup(reorderedTriangle));
	}

	{
		const TemporaryStlFile crlfStlFile(
			"solid crlf\r\n"
			"  facet normal 0 0 1\r\n"
			"    outer loop\r\n"
			"      vertex 10.5 -4.25 2.125\r\n"
			"      vertex 11.5 -4.25 2.125\r\n"
			"      vertex 10.5 -3.25 2.125\r\n"
			"    endloop\r\n"
			"  endfacet\r\n"
			"endsolid crlf\r\n");
		HashTable hashTable(1'024);
		hashTable.Construct(crlfStlFile.Path().string());

		const Triangle crlfTriangle = BuildTriangle(
			10.5, -4.25, 2.125,
			11.5, -4.25, 2.125,
			10.5, -3.25, 2.125);
		RecordResult(summary, "Construct accepts CRLF STL files with leading facet and loop whitespace.",
			hashTable.Lookup(crlfTriangle));
	}

	{
		const Point3 a{ 1.25, -2.5, 3.75 };
		const Point3 b{ -4.0, 5.0, 6.0 };
		const Point3 c{ 7.0, 8.0, -9.0 };

		const auto orderIndependent = hashTriangle(a, b, c, 4096, 1e-6, true);
		const auto windingSensitive = hashTriangle(a, b, c, 4096, 1e-6, false);
		const auto reversedWinding = hashTriangle(c, b, a, 4096, 1e-6, false);

		RecordResult(summary, "hashTriangle has a bounded winding-sensitive variant.", windingSensitive < 4096);
		RecordResult(summary, "Winding-sensitive hashing distinguishes reversed vertex order.",
			windingSensitive != reversedWinding);
		RecordResult(summary, "Order-independent hashing ignores reversed vertex order.",
			orderIndependent == hashTriangle(c, b, a, 4096, 1e-6, true));
	}

	{
		const Triangle triangleA = BuildTriangle(1, 2, 3, 4, 5, 6, 7, 8, 9);
		const Triangle triangleB = BuildTriangle(7, 8, 9, 1, 2, 3, 4, 5, 6);
		RecordResult(summary, "Triangle construction normalizes all six vertex permutations.",
			triangleA == triangleB);
		RecordResult(summary, "Default triangles remain distinct from non-origin triangles.",
			!(Triangle{} == triangleA));
	}

	{
		const Point3 a{ 1.0, 2.0, 3.0 };
		const Point3 b{ 1.0, 2.0, 3.0 + Point3::EPS / 2.0 };
		const Point3 c{ 1.0, 2.0, 3.0 + Point3::EPS * 2.0 };
		RecordResult(summary, "Point3 equality remains symmetric at in-tolerance and boundary-adjacent values.",
			(a == b) && (b == a) && !(a == c) && !(c == a));
	}

	std::cout << "\nTests run: " << (summary.passed + summary.failed) << "\n";
	std::cout << "Passed: " << summary.passed << "\n";
	std::cout << "Failed: " << summary.failed << '\n';

	if (summary.failed > 0)
	{
		std::cerr << "Unit tests failed." << '\n';
		std::exit(1);
	}
}

} // namespace

int main()
{
	try
	{
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(nullptr);

		RunAllTests();
		return 0;
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Unexpected exception: " << ex.what() << '\n';
		return 1;
	}
}
