#include <iostream>
#include <chrono>
#include <filesystem>

#include "Triangle.h"
#include "HashTable.h"


static constexpr auto MAX_TRIANGLES = 1000000; // 1 Million

static constexpr auto CMD_LINE_USAGE_STR = "GeometricHashTable.exe wavy_surface_100000.stl {{0.000000e+00,0.000000e+00,0.000000e+00}{1.000000e+00,1.000000e+00,6.646931e-01}{0.000000e+00,1.000000e+00,0.000000e+00}}";

static constexpr auto BRACE_LEN = 1; // Length of '{' or '}' character.
static constexpr auto COMMA_LEN = 1; // Length of ',' character.


static inline void PrintUsage()
{
	std::cout << "Invalid input arguments." << '\n';

	std::cout << "Usage ==> GeometricHashTable.exe <Geometric STL file> <Triangle to lookup>" << '\n' << '\n';
	std::cout << "Example ==> " << "\"" << CMD_LINE_USAGE_STR << "\"" << '\n';
}

// Parse and construct triangle from command line input.

// Example command line input below that represents a triangle.
// "{{0.000000e+00,0.000000e+00,0.000000e+00}{1.000000e+00,1.000000e+00,6.646931e-01}{0.000000e+00,1.000000e+00,0.000000e+00}}"

static Triangle GetTriangle(const std::string_view& triangleStr)
{
	Point3 p1, p2, p3;

	detail::from_chars_result res;
	res = std::from_chars(triangleStr.data() + 2 * BRACE_LEN, triangleStr.data() + triangleStr.size(), p1.x);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p1.y);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p1.z);

	res = std::from_chars(res.ptr + 2 * BRACE_LEN, triangleStr.data() + triangleStr.size(), p2.x);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p2.y);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p2.z);

	res = std::from_chars(res.ptr + 2 * BRACE_LEN, triangleStr.data() + triangleStr.size(), p3.x);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p3.y);

	res = std::from_chars(res.ptr + COMMA_LEN, triangleStr.data() + triangleStr.size(), p3.z);

	return { p1, p2, p3 };
}

static inline bool AreValidInputArgs(const std::string& stlFileStr)
{
	return std::filesystem::exists(stlFileStr);
}


int main(int argc, char* argv[])
{
	try
	{
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(nullptr);

		if (argc != 3)
		{
			PrintUsage();
			return EXIT_FAILURE;
		}

		const std::string& stlFileStr = argv[1];
		
		if (!AreValidInputArgs(stlFileStr))
		{
			PrintUsage();
			return EXIT_FAILURE;
		}

		HashTable hashTable(MAX_TRIANGLES);

		auto start = std::chrono::steady_clock::now();
		hashTable.Construct(stlFileStr);
		auto end = std::chrono::steady_clock::now();

		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::cout << "Time taken for Hash table construction: " << elapsedTime << " MILLIseconds" << '\n';

		const auto triangle = GetTriangle(argv[2]);

		start = std::chrono::steady_clock::now();
		auto triangleExists = hashTable.Lookup(triangle);
		end = std::chrono::steady_clock::now();

		std::cout << "Given triangle " << (triangleExists ? "" : "NOT ") << "exists." << '\n';

		elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
		std::cout << "Time taken for given triangle Lookup: " << elapsedTime << " NANOseconds" << '\n';

		return EXIT_SUCCESS;
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Exception occurred -- " << ex.what() << '\n';
	}
	catch (...)
	{
		std::cerr << "Unknown exception occurred." << '\n';
	}

	return EXIT_FAILURE;
}

