#pragma once


#include <vector>
#include <optional>


// Dense, flat, memory efficient, cache friendly, super fast Hash table targeting higher Cache-hit rates.
// Having given that Hash function is collision-free, implementation is heavily optimized for Lookups.


class HashTable
{
public:
	explicit HashTable(const uint32_t maxEntries);

	void Construct(const std::string& stlFileStr);
	bool Lookup(const Triangle& triangle) const;

	HashTable(const HashTable&) = delete;
	HashTable(HashTable&&) = delete;
	HashTable& operator=(const HashTable&) = delete;
	HashTable& operator=(HashTable&&) = delete;

private:
	void _Initialize(const std::string& stlFileStr, std::ifstream& stlFileStream);

	void _ConstructTriangle(std::ifstream& stlFileStream);

private:
	uint32_t m_maxEntries = 0;

	std::vector<std::optional<Triangle>> m_Triangles;
};