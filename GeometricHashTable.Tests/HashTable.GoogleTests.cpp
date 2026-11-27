#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "../GeometricHashTable/Triangle.h"
#include "../GeometricHashTable/Hash.h"
#include "../GeometricHashTable/HashTable.h"

namespace {

class TemporaryStlFile {
public:
	explicit TemporaryStlFile(const std::string& contents)
		: m_path(std::filesystem::temp_directory_path() /
			("GeometricHashTable.GoogleTests." +
			 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".stl")) {
		std::ofstream file(m_path);
		if (!file) {
			throw std::runtime_error("Unable to create temporary STL file: " + m_path.string());
		}
		file << contents;
		if (!file) {
			file.close();
			std::error_code error;
			std::filesystem::remove(m_path, error);
			throw std::runtime_error("Unable to write temporary STL file: " + m_path.string());
		}
	}

	~TemporaryStlFile() {
		std::error_code error;
		std::filesystem::remove(m_path, error);
	}

	const std::filesystem::path& path() const { return m_path; }

private:
	std::filesystem::path m_path;
};

Triangle BuildTriangle(double x1, double y1, double z1,
					   double x2, double y2, double z2,
					   double x3, double y3, double z3) {
	return Triangle{ Point3{ x1, y1, z1 }, Point3{ x2, y2, z2 }, Point3{ x3, y3, z3 } };
}

std::filesystem::path WavySurfacePath() {
	return std::filesystem::path(__FILE__).parent_path().parent_path() /
		"GeometricHashTable" / "wavy_surface_100000.stl";
}

} // namespace

TEST(Point3Tests, DefaultAndToleranceEquality) {
	const Point3 origin;
	const Point3 zero{ 0.0, 0.0, 0.0 };
	const Point3 nearby{ detail::EPS / 2.0, 0.0, 0.0 };
	const Point3 boundary{ detail::EPS, 0.0, 0.0 };
	EXPECT_EQ(origin, zero);
	EXPECT_TRUE(origin == nearby);
	EXPECT_FALSE(origin == boundary);
}

TEST(Point3Tests, LexicographicOrdering) {
	const Point3 zBefore{ 1.0, 2.0, 3.0 };
	const Point3 zAfter{ 1.0, 2.0, 4.0 };
	const Point3 yBefore{ 1.0, 2.0, 9.0 };
	const Point3 yAfter{ 1.0, 3.0, 0.0 };
	EXPECT_LT(zBefore, zAfter);
	EXPECT_LT(yBefore, yAfter);
	const Point3 origin;
	EXPECT_FALSE(origin < origin);
}

TEST(TriangleTests, ConstructorNormalizesAndDefaultIsOrigin) {
	Triangle normalized = BuildTriangle(3.0, 3.0, 3.0, 1.0, 1.0, 1.0, 2.0, 2.0, 2.0);
	const Point3 first{ 1.0, 1.0, 1.0 };
	const Point3 second{ 2.0, 2.0, 2.0 };
	const Point3 third{ 3.0, 3.0, 3.0 };
	EXPECT_EQ(normalized[0], first);
	EXPECT_EQ(normalized[1], second);
	EXPECT_EQ(normalized[2], third);

	Triangle defaultTriangle;
	const Triangle originTriangle{ Point3{}, Point3{}, Point3{} };
	EXPECT_EQ(defaultTriangle, originTriangle);
}

TEST(VertexParsingTests, AcceptsPositiveNegativeAndScientificCoordinates) {
	Point3 point;
	GetVertex("vertex 1.25 -2.5 3.75", &point);
	const Point3 positiveNegative{ 1.25, -2.5, 3.75 };
	EXPECT_EQ(point, positiveNegative);

	GetVertex("vertex -1.25e-2 2.5e+1 3.75e0", &point);
	const Point3 scientific{ -0.0125, 25.0, 3.75 };
	EXPECT_EQ(point, scientific);
}

TEST(VertexParsingTests, RejectsMalformedAndMissingInput) {
	EXPECT_THROW(GetVertex("facet normal 0 0 0", reinterpret_cast<Point3*>(0)), std::invalid_argument);

	Point3 point;
	EXPECT_THROW(GetVertex("not-a-vertex 0 1 2", &point), std::invalid_argument);
	EXPECT_THROW(GetVertex("vertex invalid 0 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 invalid 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 2 invalid", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 2 ", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 2 3", nullptr), std::invalid_argument);
}

TEST(HashTests, TriangleHashIsOrderIndependentAndBounded) {
	const Point3 a{ 1.25, -2.5, 3.75 };
	const Point3 b{ -4.0, 5.0, 6.0 };
	const Point3 c{ 7.0, 8.0, -9.0 };

	const std::uint32_t expected = hashTriangle(a, b, c, 4096);
	EXPECT_EQ(expected, hashTriangle(a, c, b, 4096));
	EXPECT_EQ(expected, hashTriangle(b, a, c, 4096));
	EXPECT_EQ(expected, hashTriangle(b, c, a, 4096));
	EXPECT_EQ(expected, hashTriangle(c, a, b, 4096));
	EXPECT_EQ(expected, hashTriangle(c, b, a, 4096));
	EXPECT_LT(expected, 4096u);
}

TEST(HashTests, QuantizationUsesToleranceAndHashHelpersAreStable) {
	EXPECT_EQ(quantize(1.0000004, 1e-6), quantize(1.0000001, 1e-6));
	EXPECT_EQ(quantize(1.0000005, 1e-6), quantize(1.0, 1e-6) + 1);
	EXPECT_EQ(quantize(-1.0000004, 1e-6), quantize(-1.0000001, 1e-6));

	const Point3 point{ -7.25, 0.5, 12.75 };
	EXPECT_EQ(hashPoint(point, 1e-6), hashPoint(point, 1e-6));
	EXPECT_NE(mix64(0), mix64(1));
}

TEST(HashTests, WindingVariantDistinguishesReversedVertexOrder) {
	const Point3 a{ 1.25, -2.5, 3.75 };
	const Point3 b{ -4.0, 5.0, 6.0 };
	const Point3 c{ 7.0, 8.0, -9.0 };

	const std::uint32_t forward = hashTriangle(a, b, c, 4096, 1e-6, false);
	const std::uint32_t reverse = hashTriangle(c, b, a, 4096, 1e-6, false);
	EXPECT_LT(forward, 4096u);
	EXPECT_NE(forward, reverse);
}

TEST(HashTableTests, MissingFileAndEmptyStlAreHandled) {
	HashTable table(256);
	EXPECT_THROW(table.Construct("this_file_should_not_exist.stl"), std::invalid_argument);

	TemporaryStlFile empty("solid empty\nendsolid empty\n");
	table.Construct(empty.path().string());
	EXPECT_FALSE(table.Lookup(BuildTriangle(1, 1, 1, 2, 1, 1, 1, 2, 1)));
}

TEST(HashTableTests, ConstructsAndLooksUpMinimalAsciiStl) {
	TemporaryStlFile file(
		"solid test\n"
		"  outer loop\n"
		"   vertex 0 0 0\n"
		"   vertex 0 1 0\n"
		"   vertex 1 0 0\n"
		"  endloop\n"
		"endsolid test\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());

	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 0, 1, 0, 1, 0, 0)));
	EXPECT_TRUE(table.Lookup(BuildTriangle(1, 0, 0, 0, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, LookupHonorsToleranceAndQuantization) {
	TemporaryStlFile file(
		"solid tolerance\n"
		"  outer loop\n"
		"   vertex 0 0 0\n"
		"   vertex 0 1 0\n"
		"   vertex 1 0 0\n"
		"  endloop\n"
		"endsolid tolerance\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());

	Triangle stored = BuildTriangle(0, 0, 0, 0, 1, 0, 1, 0, 0);
	Triangle near = BuildTriangle(0, 0, 0, 0, 1, 0, 1.0 + detail::EPS / 10.0, 0, 0);
	Triangle outside = BuildTriangle(0, 0, 0, 0, 1, 0, 1.0 + detail::EPS * 2.0, 0, 0);

	EXPECT_TRUE(table.Lookup(stored));
	EXPECT_TRUE(table.Lookup(near));
	EXPECT_FALSE(table.Lookup(outside));
}

TEST(HashTableTests, DuplicateAndDistinctTrianglesAreRetained) {
	TemporaryStlFile file(
		"solid triangles\n"
		"  outer loop\n"
		"   vertex 0 0 0\n"
		"   vertex 2 0 0\n"
		"   vertex 0 2 0\n"
		"  endloop\n"
		"  outer loop\n"
		"   vertex 0 2 0\n"
		"   vertex 0 0 0\n"
		"   vertex 2 0 0\n"
		"  endloop\n"
		"  outer loop\n"
		"   vertex 5 5 0\n"
		"   vertex 6 5 0\n"
		"   vertex 5 6 0\n"
		"  endloop\n"
		"endsolid triangles\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(2, 0, 0, 0, 2, 0, 0, 0, 0)));
	EXPECT_TRUE(table.Lookup(BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0)));
}

TEST(HashTableTests, RepeatedConstructionAddsDistinctBuckets) {
	TemporaryStlFile first(
		"solid first\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\nendsolid first\n");
	TemporaryStlFile second(
		"solid second\n  outer loop\n   vertex 5 5 0\n   vertex 6 5 0\n   vertex 5 6 0\n  endloop\nendsolid second\n");

	HashTable table(1'000'000);
	table.Construct(first.path().string());
	table.Construct(second.path().string());

	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
	EXPECT_TRUE(table.Lookup(BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0)));
}

TEST(HashTableTests, FailedReconstructionPreservesExistingLookups) {
	TemporaryStlFile file(
		"solid state\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\nendsolid state\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_THROW(table.Construct("missing_reconstruction_file.stl"), std::invalid_argument);
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, CollisionsRetainOnlyPrimarySlot) {
	TemporaryStlFile file(
		"solid collisions\n"
		"  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n"
		"  outer loop\n   vertex 5 5 0\n   vertex 6 5 0\n   vertex 5 6 0\n  endloop\n"
		"endsolid collisions\n");

	HashTable table(1);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, MalformedAndIncompleteFacetsThrow) {
	TemporaryStlFile malformed(
		"solid malformed\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   not-a-vertex 0 1 0\n  endloop\nendsolid malformed\n");
	HashTable malformedTable(1'000'000);
	EXPECT_THROW(malformedTable.Construct(malformed.path().string()), std::invalid_argument);

	TemporaryStlFile incomplete(
		"solid incomplete\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n  endloop\nendsolid incomplete\n");
	HashTable incompleteTable(1'000'000);
	EXPECT_THROW(incompleteTable.Construct(incomplete.path().string()), std::invalid_argument);
}

TEST(HashTableTests, SupportsNegativeCoordinatesAndDegenerateTriangles) {
	TemporaryStlFile file(
		"solid edge\n"
		"  outer loop\n   vertex -3.5 -2.25 1.125\n   vertex -2.5 -2.25 1.125\n   vertex -3.5 -1.25 1.125\n  endloop\n"
		"  outer loop\n   vertex 0 0 0\n   vertex 0 0 0\n   vertex 0 0 0\n  endloop\n"
		"endsolid edge\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(-3.5, -2.25, 1.125, -2.5, -2.25, 1.125, -3.5, -1.25, 1.125)));
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 0, 0, 0, 0, 0, 0)));
}

TEST(HashTableTests, LoadsKnownTriangleAndCRLFFromWavyFixture) {
	HashTable table(1'000'000);
	table.Construct(WavySurfacePath().string());

	Triangle known = BuildTriangle(0, 0, 0, 1, 0, 0.6661729, 1, 1, 0.6646931);
	Triangle missing = BuildTriangle(9, 9, 9, 9, 9, 9.1, 9, 9.1, 9);
	EXPECT_TRUE(table.Lookup(known));
	EXPECT_FALSE(table.Lookup(missing));
}

TEST(HashTests, SixPermutationsRemainStableAcrossCoordinateGrid) {
	for (int x = -3; x <= 3; ++x) {
		for (int y = -2; y <= 2; ++y) {
			for (int z = -1; z <= 1; ++z) {
				const Point3 p1{ x * 0.125, y * 0.25, z * 0.5 };
				const Point3 p2{ x * 0.125 + 0.75, y * 0.25 - 0.5, z * 0.5 + 0.25 };
				const Point3 p3{ x * 0.125 - 0.25, y * 0.25 + 0.125, z * 0.5 - 0.75 };
				const std::uint32_t expected = hashTriangle(p1, p2, p3, 4096);
				EXPECT_EQ(expected, hashTriangle(p1, p3, p2, 4096));
				EXPECT_EQ(expected, hashTriangle(p2, p1, p3, 4096));
				EXPECT_EQ(expected, hashTriangle(p2, p3, p1, 4096));
				EXPECT_EQ(expected, hashTriangle(p3, p1, p2, 4096));
				EXPECT_EQ(expected, hashTriangle(p3, p2, p1, 4096));
				EXPECT_LT(expected, 4096u);
			}
		}
	}
}

TEST(HashTableTests, EmptyReconstructionRetainsPriorBuckets) {
	TemporaryStlFile first(
		"solid first\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\nendsolid first\n");
	TemporaryStlFile empty("solid empty\nendsolid empty\n");

	HashTable table(1'000'000);
	table.Construct(first.path().string());
	table.Construct(empty.path().string());

	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, NegativeCoordinateLookupRejectsOutsideTolerance) {
	TemporaryStlFile file(
		"solid negative\n  outer loop\n   vertex -3.5 -2.25 1.125\n   vertex -2.5 -2.25 1.125\n   vertex -3.5 -1.25 1.125\n  endloop\nendsolid negative\n");

	HashTable table(1'000'000);
	table.Construct(file.path().string());
	const Triangle outside = BuildTriangle(
		-3.5, -2.25, 1.125,
		-2.5, -2.25, 1.125,
		-3.5, -1.25, 1.125 + detail::EPS * 2.0);
	EXPECT_FALSE(table.Lookup(outside));
}

TEST(HashTableTests, NegativeCoordinatesWithinToleranceRemainLookupable) {
	TemporaryStlFile file(
		"solid negative_near\n"
		" outer loop\n"
		"  vertex -3.5 -2.25 1.125\n"
		"  vertex -2.5 -2.25 1.125\n"
		"  vertex -3.5 -1.25 1.125\n"
		" endloop\n"
		"endsolid negative_near\n");
	HashTable table(1'000'000);
	table.Construct(file.path().string());

	const Triangle nearby = BuildTriangle(
		-3.5, -2.25, 1.125 - detail::EPS / 10.0,
		-2.5, -2.25, 1.125,
		-3.5, -1.25, 1.125);
	EXPECT_TRUE(table.Lookup(nearby));
}

TEST(HashTableTests, AcquiresCRLFSTLContent) {
	TemporaryStlFile file(
		"solid crlf\r\n"
		"  facet normal 0 0 1\r\n"
		"    outer loop\r\n"
		"      vertex 10.5 -4.25 2.125\r\n"
		"      vertex 11.5 -4.25 2.125\r\n"
		"      vertex 10.5 -3.25 2.125\r\n"
		"    endloop\r\n"
		"  endfacet\r\n"
		"endsolid crlf\r\n");

	HashTable table(1'024);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(10.5, -4.25, 2.125, 11.5, -4.25, 2.125, 10.5, -3.25, 2.125)));
}

TEST(Point3Tests, EqualityRejectsEachCoordinateMismatch) {
	const Point3 point{ 1.0, 2.0, 3.0 };
	const Point3 xMismatch{ 2.0, 2.0, 3.0 };
	const Point3 yMismatch{ 1.0, 3.0, 3.0 };
	const Point3 zMismatch{ 1.0, 2.0, 4.0 };
	EXPECT_FALSE(point == xMismatch);
	EXPECT_FALSE(point == yMismatch);
	EXPECT_FALSE(point == zMismatch);
}

TEST(Point3Tests, EqualityIsSymmetricAtToleranceBoundary) {
	const Point3 point{ 1.0, 2.0, 3.0 };
	const Point3 nearby{ 1.0, 2.0, 3.0 + detail::EPS / 2.0 };
	const Point3 outside{ 1.0, 2.0, 3.0 + detail::EPS * 2.0 };
	EXPECT_TRUE(point == nearby);
	EXPECT_TRUE(nearby == point);
	EXPECT_FALSE(point == outside);
	EXPECT_FALSE(outside == point);
}

TEST(Point3Tests, ToleranceBoundaryIsStrictForBothSignsOnEveryAxis) {
	const Point3 origin;
	const std::array<Point3, 6> boundaryPoints = {{
		{ detail::EPS, 0.0, 0.0 },
		{ -detail::EPS, 0.0, 0.0 },
		{ 0.0, detail::EPS, 0.0 },
		{ 0.0, -detail::EPS, 0.0 },
		{ 0.0, 0.0, detail::EPS },
		{ 0.0, 0.0, -detail::EPS }
	}};
	for (const Point3& point : boundaryPoints)
		EXPECT_FALSE(origin == point);
}

TEST(Point3Tests, OrderingUsesXThenYThenZ) {
	const Point3 xBefore{ 0.0, 9.0, 9.0 };
	const Point3 xAfter{ 1.0, -9.0, -9.0 };
	const Point3 yBefore{ 1.0, 0.0, 9.0 };
	const Point3 yAfter{ 1.0, 1.0, -9.0 };
	const Point3 zBefore{ 1.0, 1.0, 0.0 };
	const Point3 zAfter{ 1.0, 1.0, 1.0 };
	EXPECT_LT(xBefore, xAfter);
	EXPECT_LT(yBefore, yAfter);
	EXPECT_LT(zBefore, zAfter);
}

TEST(TriangleTests, AllSixPermutationsNormalizeToSameTriangle) {
	const Triangle triangle = BuildTriangle(1, 2, 3, 4, 5, 6, 7, 8, 9);
	const std::array<Triangle, 6> permutations = {{
		BuildTriangle(1, 2, 3, 4, 5, 6, 7, 8, 9),
		BuildTriangle(1, 2, 3, 7, 8, 9, 4, 5, 6),
		BuildTriangle(4, 5, 6, 1, 2, 3, 7, 8, 9),
		BuildTriangle(4, 5, 6, 7, 8, 9, 1, 2, 3),
		BuildTriangle(7, 8, 9, 1, 2, 3, 4, 5, 6),
		BuildTriangle(7, 8, 9, 4, 5, 6, 1, 2, 3)
	}};
	for (const Triangle& permutation : permutations)
		EXPECT_EQ(triangle, permutation);
}

TEST(TriangleTests, DefaultTriangleDiffersFromNonOriginTriangle) {
	const Triangle defaultTriangle;
	const Triangle nonOrigin = BuildTriangle(1, 2, 3, 4, 5, 6, 7, 8, 9);
	EXPECT_FALSE(defaultTriangle == nonOrigin);
}

TEST(TriangleTests, NormalizationPreservesDuplicateVertices) {
	const Triangle triangle = BuildTriangle(2, 0, 0, 1, 0, 0, 1, 0, 0);
	const Point3 repeated{ 1, 0, 0 };
	const Point3 distinct{ 2, 0, 0 };
	EXPECT_EQ(triangle[0], repeated);
	EXPECT_EQ(triangle[1], repeated);
	EXPECT_EQ(triangle[2], distinct);
}

TEST(VertexParsingTests, RejectsIncompleteScientificNotationBeforeFinalCoordinate) {
	Point3 point;
	EXPECT_THROW(GetVertex("vertex 1e 0 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 2e 0", &point), std::runtime_error);
}

TEST(VertexParsingTests, RejectsMalformedYAndZCoordinates) {
	Point3 point;
	EXPECT_THROW(GetVertex("vertex 1 invalid 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 1 2 invalid", &point), std::runtime_error);
}

TEST(VertexParsingTests, RejectsMissingZCoordinateAndCoordinateOnlyLine) {
	Point3 point;
	EXPECT_THROW(GetVertex("vertex 1 2 ", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex ", &point), std::runtime_error);
}

TEST(VertexParsingTests, RejectsNullPointer) {
	EXPECT_THROW(GetVertex("vertex 1 2 3", nullptr), std::invalid_argument);
}

TEST(HashTests, QuantizesZeroAndNegativeHalfToleranceDeterministically) {
	EXPECT_EQ(quantize(0.0, 1e-6), 0);
	EXPECT_EQ(quantize(-0.0000005, 1e-6), -1);
	EXPECT_EQ(quantize(0.0000005, 1e-6), 1);
	EXPECT_EQ(quantize(-1.0000005, 1e-6), quantize(-1.000001, 1e-6));
}

TEST(HashTests, OneBucketAndSmallTableSizesStayInRange) {
	const Point3 a{ 1.25, -2.5, 3.75 };
	const Point3 b{ -4.0, 5.0, 6.0 };
	const Point3 c{ 7.0, 8.0, -9.0 };
	EXPECT_EQ(hashTriangle(a, b, c, 1), 0u);
	for (const std::uint32_t tableSize : { 2u, 3u, 7u, 64u })
		EXPECT_LT(hashTriangle(a, b, c, tableSize), tableSize);
}

TEST(HashTests, HashPointIsStableAndDistinguishesAxisChanges) {
	const Point3 base{ 1.0, 2.0, 3.0 };
	const Point3 xChanged{ 1.1, 2.0, 3.0 };
	const Point3 yChanged{ 1.0, 2.1, 3.0 };
	const Point3 zChanged{ 1.0, 2.0, 3.1 };
	const uint64_t baseHash = hashPoint(base, 1e-6);
	EXPECT_EQ(baseHash, hashPoint(base, 1e-6));
	EXPECT_NE(baseHash, hashPoint(xChanged, 1e-6));
	EXPECT_NE(baseHash, hashPoint(yChanged, 1e-6));
	EXPECT_NE(baseHash, hashPoint(zChanged, 1e-6));
}

TEST(HashTests, HashHandlesZeroNegativeAndPositiveCoordinates) {
	const Point3 origin;
	EXPECT_LT(hashTriangle(origin, origin, origin, 64), 64u);
	const Point3 a{ -1.25, 2.5, -3.75 };
	const Point3 b{ 4.0, -5.0, 6.0 };
	const Point3 c{ 7.0, 8.0, -9.0 };
	EXPECT_LT(hashTriangle(a, b, c, 31), 31u);
	EXPECT_EQ(hashTriangle(a, b, c, 31), hashTriangle(a, b, c, 31));
}

TEST(HashTests, Mix64IsDeterministicForZero) {
	EXPECT_EQ(mix64(0), mix64(0));
	EXPECT_NE(mix64(0), mix64(1));
}

TEST(HashTableTests, ZeroSizedTableWithNoFacetsIsSafe) {
	TemporaryStlFile empty("solid empty\nendsolid empty\n");
	HashTable table(0);
	EXPECT_NO_THROW(table.Construct(empty.path().string()));
}

TEST(HashTableTests, FacetWithoutOuterLoopDoesNotCreateTriangle) {
	TemporaryStlFile file(
		"solid no_loop\n"
		" facet normal 0 0 1\n"
		" endfacet\n"
		"endsolid no_loop\n");
	HashTable table(1'024);
	EXPECT_NO_THROW(table.Construct(file.path().string()));
	EXPECT_FALSE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, MultipleTrianglesInOneStlAreAllLookedUp) {
	TemporaryStlFile file(
		"solid multiple\n"
		" outer loop\n  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\n"
		" outer loop\n  vertex 5 5 0\n  vertex 6 5 0\n  vertex 5 6 0\n endloop\n"
		"endsolid multiple\n");
	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
	EXPECT_TRUE(table.Lookup(BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0)));
}

TEST(HashTableTests, OneBucketCollisionDoesNotRetainSecondTriangle) {
	TemporaryStlFile file(
		"solid collision\n"
		" outer loop\n  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\n"
		" outer loop\n  vertex 5 5 0\n  vertex 6 5 0\n  vertex 5 6 0\n endloop\n"
		"endsolid collision\n");
	HashTable table(1);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
	EXPECT_FALSE(table.Lookup(BuildTriangle(5, 5, 0, 6, 5, 0, 5, 6, 0)));
}

TEST(HashTableTests, DuplicateStlFacetsRemainLookupable) {
	TemporaryStlFile file(
		"solid duplicates\n"
		" outer loop\n  vertex 0 0 0\n  vertex 2 0 0\n  vertex 0 2 0\n endloop\n"
		" outer loop\n  vertex 0 2 0\n  vertex 0 0 0\n  vertex 2 0 0\n endloop\n"
		"endsolid duplicates\n");
	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(2, 0, 0, 0, 2, 0, 0, 0, 0)));
}

TEST(HashTableTests, DefaultTriangleIsRejectedInEmptyTable) {
	TemporaryStlFile empty("solid empty\nendsolid empty\n");
	HashTable table(1'024);
	table.Construct(empty.path().string());
	const Triangle defaultTriangle;
	EXPECT_FALSE(table.Lookup(defaultTriangle));
}

TEST(HashTableTests, WavyFixtureLookupIgnoresVertexOrder) {
	HashTable table(1'000'000);
	table.Construct(WavySurfacePath().string());
	const Triangle reordered = BuildTriangle(
		1.0, 1.0, 0.6646931,
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.6661729);
	EXPECT_TRUE(table.Lookup(reordered));
}

TEST(HashTableTests, WavyFixtureReturnsFalseForAbsentTriangle) {
	HashTable table(1'000'000);
	table.Construct(WavySurfacePath().string());
	const Triangle missing = BuildTriangle(9, 9, 9, 9, 9, 9.1, 9, 9.1, 9);
	EXPECT_FALSE(table.Lookup(missing));
}

TEST(VertexParsingTests, RejectsNumericOverflowInEachCoordinate) {
	Point3 point;
	EXPECT_THROW(GetVertex("vertex 1e9999 0 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 0 1e9999 0", &point), std::runtime_error);
	EXPECT_THROW(GetVertex("vertex 0 0 1e9999", &point), std::runtime_error);
}

TEST(VertexParsingTests, RejectsEmptyString) {
	Point3 point;
	EXPECT_THROW(GetVertex("", &point), std::invalid_argument);
}

TEST(HashTests, NearbyCoordinatesInSameQuantizationCellHashIdentically) {
	const Point3 original{ 1.0, -2.0, 3.0 };
	const Point3 nearby{ 1.0 + detail::EPS / 10.0, -2.0, 3.0 };
	EXPECT_EQ(hashPoint(original, detail::EPS), hashPoint(nearby, detail::EPS));
}

TEST(HashTests, TriangleHashIsBoundedForSeveralTableSizes) {
	const Point3 a{ -13.75, 0.125, 22.5 };
	const Point3 b{ 4.5, -7.25, 8.0 };
	const Point3 c{ 9.125, 10.25, -11.5 };
	for (const std::uint32_t size : { 1u, 2u, 3u, 17u, 255u, 4096u, 1'000'000u })
		EXPECT_LT(hashTriangle(a, b, c, size), size);
}

TEST(HashTests, TriangleHashStaysInRangeNearMaximumTableSize) {
	const Point3 a{ -13.75, 0.125, 22.5 };
	const Point3 b{ 4.5, -7.25, 8.0 };
	const Point3 c{ 9.125, 10.25, -11.5 };
	for (const std::uint32_t size : { 0x80000000u, UINT32_MAX }) {
		const std::uint32_t hash = hashTriangle(a, b, c, size);
		EXPECT_LT(hash, size);
		EXPECT_EQ(hash, hashTriangle(a, b, c, size));
	}
}

TEST(HashTableTests, UnrelatedStlLinesAreIgnored) {
	TemporaryStlFile file(
		"solid ignored\n"
		" facet normal 0 0 1\n"
		" vertex 99 99 99\n"
		" endfacet\n"
		" outer loop\n"
		"  vertex 0 0 0\n"
		"  vertex 1 0 0\n"
		"  vertex 0 1 0\n"
		" endloop\n"
		"endsolid ignored\n");
	HashTable table(1'000'000);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
	EXPECT_FALSE(table.Lookup(BuildTriangle(99, 99, 99, 98, 99, 99, 99, 98, 99)));
}

TEST(HashTableTests, MalformedLaterFacetDoesNotRemoveEarlierInsertedTriangle) {
	TemporaryStlFile file(
		"solid partial\n"
		" outer loop\n  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\n"
		" outer loop\n  vertex 2 2 2\n  vertex 3 2 2\n  invalid 2 3 2\n endloop\n"
		"endsolid partial\n");
	HashTable table(1'000'000);
	EXPECT_THROW(table.Construct(file.path().string()), std::invalid_argument);
	EXPECT_TRUE(table.Lookup(BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0)));
}

TEST(HashTableTests, MalformedFacetDoesNotBecomeLookupable) {
	TemporaryStlFile file(
		"solid malformed_facet\n"
		" outer loop\n"
		"  vertex 2 2 2\n"
		"  vertex 3 2 2\n"
		"  invalid 2 3 2\n"
		" endloop\n"
		"endsolid malformed_facet\n");
	HashTable table(1'000'000);
	EXPECT_THROW(table.Construct(file.path().string()), std::invalid_argument);
	EXPECT_FALSE(table.Lookup(BuildTriangle(2, 2, 2, 3, 2, 2, 2, 3, 2)));
}

TEST(HashTableTests, ReconstructingSameFacetIsIdempotentForLookup) {
	TemporaryStlFile file(
		"solid repeated\n"
		" outer loop\n  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\n"
		"endsolid repeated\n");
	HashTable table(1'000'000);
	const Triangle triangle = BuildTriangle(0, 0, 0, 1, 0, 0, 0, 1, 0);
	table.Construct(file.path().string());
	ASSERT_TRUE(table.Lookup(triangle));
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(triangle));
}

TEST(HashTableTests, WithinToleranceLookupWorksForEachVertexCoordinate) {
	TemporaryStlFile file(
		"solid tolerance_axes\n"
		" outer loop\n  vertex 0 0 0\n  vertex 0 1 0\n  vertex 1 0 0\n endloop\n"
		"endsolid tolerance_axes\n");
	HashTable table(1'000'000);
	table.Construct(file.path().string());

	const Triangle firstVertex = BuildTriangle(0, 0, detail::EPS / 10.0, 0, 1, 0, 1, 0, 0);
	const Triangle secondVertex = BuildTriangle(0, 0, 0, 0, 1.0 + detail::EPS / 10.0, 0, 1, 0, 0);
	const Triangle thirdVertex = BuildTriangle(0, 0, 0, 0, 1, 0, 1.0 + detail::EPS / 10.0, 0, 0);
	EXPECT_TRUE(table.Lookup(firstVertex));
	EXPECT_TRUE(table.Lookup(secondVertex));
	EXPECT_TRUE(table.Lookup(thirdVertex));
}

TEST(HashTableTests, ConstructsAndLooksUpScientificNotationWithFlexibleWhitespace) {
	TemporaryStlFile file(
		"solid formatted\n\n"
		"\touter loop\n"
		"\t\tvertex 1.25e+1 -2.5e-1 3.0\n"
		"\t\tvertex 1.325e+1 -2.5e-1 3.0\n"
		"\t\tvertex 1.225e+1 7.5e-1 3.0\n"
		"\tendloop\n"
		"endsolid formatted\n");
	HashTable table(4'096);
	table.Construct(file.path().string());
	EXPECT_TRUE(table.Lookup(BuildTriangle(
		12.5, -0.25, 3.0,
		13.25, -0.25, 3.0,
		12.25, 0.75, 3.0)));
}
