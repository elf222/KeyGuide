// gameWorld/conversions.cpp
#include "gameWorld/conversions.hpp"
#include "global/global.hpp"

Vector2 PositionWorldToScreen(Vector2 position)
{
    return GetWorldToScreen2D(position, Global::Render::camera);
}

Vector2 PositionScreenToWorld(Vector2 position)
{
    return GetScreenToWorld2D(position, Global::Render::camera);
}
