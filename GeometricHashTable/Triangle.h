#pragma once


#include <array>


namespace detail
{
	static constexpr double EPSILON = 1e-6;

	static constexpr auto VERTEX_STR = "vertex";
	static constexpr auto VERTEX_LEN = 6; // Length of string "vertex".
	static constexpr auto WHITESPACE_LEN = 1; // Length of whitespace character.

	struct from_chars_result : public std::from_chars_result
	{
		using Base = std::from_chars_result;
		using Base::Base;

		from_chars_result(const std::from_chars_result& res)
			: Base(res)
		{
			if (res.ec != std::errc())
			{
				throw std::runtime_error(std::make_error_code(res.ec).message());
			}
		}
	};
}


struct Point3 
{
	double x = 0.0, y = 0.0, z = 0.0;

	bool IsAlmostEqual(const double& a, const double& b) const
	{
		return std::abs(a - b) < detail::EPSILON;
	}

	bool operator==(const Point3& other) const
	{
		return IsAlmostEqual(x, other.x) &&
			IsAlmostEqual(y, other.y) &&
			IsAlmostEqual(z, other.z);
	}

	bool operator<(const Point3& other) const
	{
		if (x != other.x)
		{
			return x < other.x;
		}

		if (y != other.y)
		{
			return y < other.y;
		}

		if (z != other.z)
		{
			return z < other.z;
		}

		return false;
	}
};

struct Triangle : public std::array<Point3, 3>
{
	using Base = std::array<Point3, 3>;
	using Base::Base;

	Triangle(const Point3& p1, const Point3& p2, const Point3& p3)
	{
		this->at(0) = p1;
		this->at(1) = p2;
		this->at(2) = p3;

		Normalize();
	}

	void Normalize()
	{
		std::sort(this->begin(), this->end());
	}
};

inline void GetVertex(const std::string_view& str, Point3& p)
{
	using namespace detail;

	if (!str.starts_with(VERTEX_STR))
	{
		throw std::invalid_argument("vertex not found.");
	}

	from_chars_result res = std::from_chars(str.data() + VERTEX_LEN + WHITESPACE_LEN, str.data() + str.size(), p.x);

	res = std::from_chars(res.ptr + WHITESPACE_LEN, str.data() + str.size(), p.y);

	res = std::from_chars(res.ptr + WHITESPACE_LEN, str.data() + str.size(), p.z);
}