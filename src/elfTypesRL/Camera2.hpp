// elfTypesRL/Camera2.hpp

#pragma once

#include "raylib.h"
#include "raymath.h"

#include "elfTypesRL/raylib_extensions.hpp"

class IntCamera2
{
  private:
    IntVector2 posCenter;
    IntVector2 posTopLeftCorner;

  public:
    constexpr IntCamera2() = default;

    constexpr IntCamera2(IntVector2 new_pos_center, IntVector2 screen_size)
        : posCenter{new_pos_center}, posTopLeftCorner{new_pos_center - (screen_size / 2)}
    {
    }

    IntVector2 center(void)
    {
        return posCenter;
    }

    IntVector2 tlCorner(void)
    {
        return posTopLeftCorner;
    }

    void updateFromCenter(IntVector2 new_pos_center, IntVector2 screen_size)
    {
        posCenter = new_pos_center;
        posTopLeftCorner = new_pos_center - ComplexConjugation(screen_size / 2);
    }

    void updateFromTlCorner(IntVector2 new_pos_top_left_corner, IntVector2 screen_size)
    {
        posCenter = new_pos_top_left_corner + ComplexConjugation(screen_size / 2);
        posTopLeftCorner = new_pos_top_left_corner;
    }
};

class Camera2
{
  private:
    Vector2 posCenter;
    Vector2 posTopLeftCorner;

  public:
    constexpr Camera2() = default;

    Camera2(Vector2 new_pos_center, Vector2 screen_size)
        : posCenter{new_pos_center},
          posTopLeftCorner{new_pos_center - ComplexConjugation(screen_size / 2.0f)}
    {
    }

    Vector2 center(void)
    {
        return posCenter;
    }

    Vector2 tlCorner(void)
    {
        return posTopLeftCorner;
    }

    void updateFromCenter(Vector2 new_pos_center, Vector2 screen_size)
    {
        posCenter = new_pos_center;
        posTopLeftCorner = new_pos_center - ComplexConjugation(screen_size / 2.0f);
    }

    void updateFromTlCorner(Vector2 new_pos_top_left_corner, Vector2 screen_size)
    {
        posCenter = new_pos_top_left_corner + ComplexConjugation(screen_size / 2.0f);
        posTopLeftCorner = new_pos_top_left_corner;
    }
};