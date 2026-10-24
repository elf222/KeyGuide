// elfTypesRL/IntVector2.hpp

#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>

#include <raylib.h>

struct IntVector2
{
    int x = 0, y = 0;

    constexpr IntVector2() = default;

    constexpr IntVector2(int x_value, int y_value) : x(x_value), y(y_value)
    {
    }

    IntVector2(Vector2 value)
        : x(static_cast<int>(std::lround(value.x))), y(static_cast<int>(std::lround(value.y)))
    {
    }

    constexpr operator Vector2() const
    {
        return {static_cast<float>(x), static_cast<float>(y)};
    }

    constexpr bool operator==(const IntVector2&) const = default;
};

// ----------------------------------------------------------------------------------------------------
// +- overloads for IntVector2
// ----------------------------------------------------------------------------------------------------

[[nodiscard]] constexpr IntVector2 operator+(IntVector2 lhs, IntVector2 rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

constexpr IntVector2& operator+=(IntVector2& lhs, IntVector2 rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

[[nodiscard]] constexpr IntVector2 operator-(IntVector2 lhs, IntVector2 rhs)
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

[[nodiscard]] constexpr IntVector2 operator-(IntVector2 value)
{
    return {-value.x, -value.y};
}

constexpr IntVector2& operator-=(IntVector2& lhs, IntVector2 rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

// ----------------------------------------------------------------------------------------------------
// */ overloads for IntVector2 and scalar + float conversions to raylib Vector2
// ----------------------------------------------------------------------------------------------------

[[nodiscard]] constexpr IntVector2 operator*(IntVector2 value, int scalar)
{
    return {value.x * scalar, value.y * scalar};
}

[[nodiscard]] constexpr IntVector2 operator*(int scalar, IntVector2 value)
{
    return value * scalar;
}

constexpr IntVector2& operator*=(IntVector2& lhs, int scalar)
{
    lhs.x *= scalar;
    lhs.y *= scalar;
    return lhs;
}

[[nodiscard]] constexpr IntVector2 operator/(IntVector2 value, int scalar)
{
    assert(scalar != 0);
    return {value.x / scalar, value.y / scalar};
}

constexpr IntVector2& operator/=(IntVector2& lhs, int scalar)
{
    assert(scalar != 0);
    lhs.x /= scalar;
    lhs.y /= scalar;
    return lhs;
}

// float conversions
[[nodiscard]] constexpr Vector2 operator*(IntVector2 value, float scalar)
{
    return {static_cast<float>(value.x) * scalar, static_cast<float>(value.y) * scalar};
}

[[nodiscard]] constexpr Vector2 operator*(float scalar, IntVector2 value)
{
    return value * scalar;
}

// ----------------------------------------------------------------------------------------------------
// overloads for arrays of IntVector2 + float conversions to raylib Vector2
// ----------------------------------------------------------------------------------------------------

template <std::size_t N>
[[nodiscard]] constexpr std::array<Vector2, N>
ConvertArrayIntVector2ToArrayVector2(const std::array<IntVector2, N>& values)
{
    std::array<Vector2, N> result{};

    for (std::size_t i = 0; i < N; ++i)
    {
        result[i] = static_cast<Vector2>(values[i]);
    }

    return result;
}

template <std::size_t N>
[[nodiscard]] constexpr std::array<Vector2, N> operator+(const std::array<IntVector2, N>& points,
                                                         Vector2 offset)
{
    std::array<Vector2, N> result{};

    for (std::size_t i = 0; i < N; ++i)
    {
        result[i] = static_cast<Vector2>(points[i]) + offset;
    }

    return result;
}

template <std::size_t N>
[[nodiscard]] constexpr std::array<Vector2, N> operator+(Vector2 offset,
                                                         const std::array<IntVector2, N>& points)
{
    return points + offset;
}

// ----------------------------------------------------------------------------------------------------
// Common 2D Maths
// ----------------------------------------------------------------------------------------------------

[[nodiscard]] constexpr IntVector2 IntVecor2Lerp(IntVector2 a, IntVector2 b, int multiplicator,
                                                 int divider)
{
    return a + (b - a) * multiplicator / divider;
}

[[nodiscard]] constexpr int AbsTaxi(IntVector2 vec)
{
    const int abs_x = vec.x < 0 ? -vec.x : vec.x;
    const int abs_y = vec.y < 0 ? -vec.y : vec.y;
    return abs_x + abs_y;
}

[[nodiscard]] constexpr int DistanceTaxi(IntVector2 vec_1, IntVector2 vec_2)
{
    return AbsTaxi(vec_1 - vec_2);
}

[[nodiscard]] constexpr int IntDot(IntVector2 lhs, IntVector2 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

[[nodiscard]] constexpr int IntCross2(IntVector2 lhs, IntVector2 rhs)
{
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

[[nodiscard]] constexpr bool IsIntLeftOfLine(IntVector2 point, IntVector2 a, IntVector2 b)
{
    return IntCross2(b - a, point - a) > 0;
}

[[nodiscard]] constexpr bool IsIntLeftOfLineThroughZero(IntVector2 point, IntVector2 direction)
{
    return IntCross2(direction, point) > 0;
}

[[nodiscard]] constexpr bool IsIntRightOfLine(IntVector2 point, IntVector2 a, IntVector2 b)
{
    return IntCross2(b - a, point - a) < 0;
}

[[nodiscard]] constexpr bool IsIntRightOfLineThroughZero(IntVector2 point, IntVector2 direction)
{
    return IntCross2(direction, point) < 0;
}

[[nodiscard]] constexpr bool IsIntOnLine(IntVector2 point, IntVector2 a, IntVector2 b)
{
    return IntCross2(b - a, point - a) == 0;
}

// complex stuff
[[nodiscard]] constexpr IntVector2 ComplexConjugation(IntVector2 value)
{
    value.y = -value.y;
    return value;
}

[[nodiscard]] constexpr IntVector2 ComplexMultiplication(IntVector2 lhs, IntVector2 rhs)
{
    return {lhs.x * rhs.x - lhs.y * rhs.y, lhs.x * rhs.y + lhs.y * rhs.x};
}

// ----------------------------------------------------------------------------------------------------
// Output overloads
// ----------------------------------------------------------------------------------------------------

inline std::ostream& operator<<(std::ostream& stream, const IntVector2& value)
{
    return stream << '(' << value.x << ", " << value.y << ')';
}

inline std::string ToString(const IntVector2& value)
{
    return "(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ")";
}
