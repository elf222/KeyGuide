// elfTypesRL/raylib_extensions.hpp

#pragma once

#include <array>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "raylib.h"
#include "elfTypesRL/IntVector2.hpp"

[[nodiscard]] constexpr Vector2 ComplexConjugation(Vector2 value)
{
    value.y = -value.y;
    return value;
}

[[nodiscard]] constexpr Vector2 ComplexMultiplication(IntVector2 lhs, Vector2 rhs)
{
    return {lhs.x * rhs.x - lhs.y * rhs.y, lhs.x * rhs.y + lhs.y * rhs.x};
}

[[nodiscard]] constexpr Vector2 ComplexMultiplication(Vector2 lhs, IntVector2 rhs)
{
    return ComplexMultiplication(rhs, lhs);
}

inline constexpr Vector2 ComplexMultiplication(Vector2 vec1, Vector2 vec2)
{
    return {vec1.x * vec2.x - vec1.y * vec2.y, vec1.x * vec2.y + vec1.y * vec2.x};
}

// ------------------------------------------------------------
// + vector<Vector2>  overloads
// ------------------------------------------------------------

constexpr std::vector<Vector2> operator+(const std::vector<Vector2>& points, const Vector2& offset)
{
    std::vector<Vector2> result = points;

    for (Vector2& p : result)
    {
        p.x += offset.x;
        p.y += offset.y;
    }

    return result;
}

constexpr std::vector<Vector2> operator+(const Vector2& offset, const std::vector<Vector2>& points)
{
    return points + offset;
}

constexpr std::vector<Vector2>& operator+=(std::vector<Vector2>& points, const Vector2& offset)
{
    for (Vector2& p : points)
    {
        p.x += offset.x;
        p.y += offset.y;
    }

    return points;
}

// ------------------------------------------------------------
// + array<Vector2, N>  overloads
// ------------------------------------------------------------

template <std::size_t N>
constexpr std::array<Vector2, N> operator+(const std::array<Vector2, N>& points,
                                           const Vector2& offset)
{
    auto result = points;

    for (Vector2& p : result)
    {
        p.x += offset.x;
        p.y += offset.y;
    }

    return result;
}

template <std::size_t N>
constexpr std::array<Vector2, N> operator+(const Vector2& offset,
                                           const std::array<Vector2, N>& points)
{
    return points + offset;
}

template <std::size_t N>
constexpr std::array<Vector2, N>& operator+=(std::array<Vector2, N>& points, const Vector2& offset)
{
    for (Vector2& p : points)
    {
        p.x += offset.x;
        p.y += offset.y;
    }

    return points;
}

// ------------------------------------------------------------
// * array<T, N> overloads
// ------------------------------------------------------------

template <typename T, std::size_t N>
constexpr std::array<T, N> operator*(std::array<T, N> arr, int s)
{
    for (T& value : arr)
    {
        value = value * s;
    }

    return arr;
}

template <typename T> constexpr std::vector<T> operator*(std::vector<T> vec, float s)
{
    for (T& value : vec)
    {
        value = value * s;
    }

    return vec;
}

// ============================================================
// custom output
// ============================================================

//
// lists, arrays, vectors
//

// for std::array<T, N>
template <typename T, std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const std::array<T, N>& arr)
{
    os << "{\n";

    for (std::size_t i = 0; i < N; ++i)
    {
        os << "    [" << i << "] : " << arr[i];

        if (i + 1 != N)
        {
            os << '\n';
        }
    }

    os << "\n}";

    return os;
}

template <typename T, std::size_t N> inline std::string ToString(const std::array<T, N>& arr)
{
    std::ostringstream ss;
    ss << arr;
    return ss.str();
}

// for std::vector<T, N>
template <typename T> inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec)
{
    os << "{\n";

    for (std::size_t i = 0; i < vec.size(); ++i)
    {
        os << "    [" << i << "] : " << vec[i];

        if (i + 1 != vec.size())
        {
            os << '\n';
        }
    }

    os << "\n}";

    return os;
}

template <typename T> inline std::string ToString(const std::vector<T>& vec)
{
    std::ostringstream ss;
    ss << vec;
    return ss.str();
}

//
// Custom types
//

// Vector2
inline std::ostream& operator<<(std::ostream& os, const Vector2& v)
{
    return os << '(' << v.x << ", " << v.y << ')';
}
