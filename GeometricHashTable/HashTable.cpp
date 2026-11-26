#include <algorithm>
#include <fstream>
#include <string>
#include <charconv>

#include "Triangle.h"
#include "Hash.h"
#include "HashTable.h"


static constexpr auto OUTER_LOOP_STR = "outer loop";


HashTable::HashTable(const uint32_t maxEntries)
	: m_maxEntries(maxEntries) { }

static inline uint32_t Hash(const Triangle& triangle, const uint32_t maxEntries)
{
	return hashTriangle(triangle[0], triangle[1], triangle[2],
		maxEntries, detail::EPS, false);
}

void HashTable::Construct(const std::string& stlFileStr)
{
	std::ifstream stlFileStream;
	_Initialize(stlFileStr, stlFileStream);

	std::string line;
	Triangle triangle;

	while (std::getline(stlFileStream >> std::ws, line))
	{
		if (line.starts_with(OUTER_LOOP_STR))
		{
			triangle = _GetTriangle(stlFileStream);

			auto& slot = m_Triangles[Hash(triangle, m_maxEntries)];
			if (!slot)
			{
				slot = triangle;
			}
		}
	}
}

void HashTable::_Initialize(const std::string& stlFileStr, std::ifstream& stlFileStream)
{
	stlFileStream.rdbuf()->pubsetbuf(nullptr, 0);

	stlFileStream.open(stlFileStr);
	if (!stlFileStream.is_open())
	{
		throw std::invalid_argument("Unable to open given STL file.");
	}
	 
	m_Triangles.resize(m_maxEntries);
}

Triangle HashTable::_GetTriangle(std::ifstream& stlFileStream) const
{
	Point3 p1, p2, p3;
	std::string line;

	for (Point3* p : { &p1, &p2, &p3 })
	{
		if (std::getline(stlFileStream >> std::ws, line))
		{
			GetVertex(line, p);
		}
	}

	return { p1, p2, p3 };
}

bool HashTable::Lookup(const Triangle& triangle) const
{
	const auto& otherTriangle = m_Triangles[Hash(triangle, m_maxEntries)];
	return otherTriangle && *otherTriangle == triangle;
}
