// registry/ecs_components.hpp
#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "raylib.h"
#include "gameLogic/shortcut_bindings.hpp"
#include "gameLogic/keyboard_presentation.hpp"

using Entity = int;
using Command = int;
enum class TagName
{
    Key,
    Tracker,
    Count
};
constexpr std::size_t TagCount = static_cast<std::size_t>(TagName::Count);
enum class ComponentType
{
    Entity_Position,
    Entity_Size,
    Key_Layout,
    Colour,
    Key_Values,
    Key_Hint,
    Key_Display,
    Tracked_State,
    Count
};
constexpr std::size_t ComponentCount = static_cast<std::size_t>(ComponentType::Count);
template <typename T> struct ComponentID;

struct Entity_Position
{
    Vector2 pos{};
};

struct Entity_Size
{
    Vector2 size{};
};

struct Key_Layout
{
    Vector2 pos{};
    Vector2 size{};
    int row = 0;
};

struct Colour
{
    Color col{255, 255, 255, 255};
    constexpr Colour() = default;

    constexpr Colour(Color value) : col(value)
    {
    }

    constexpr Colour(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        : col{r, g, b, a}
    {
    }
};

struct Key_Values
{
    Platform::KeyId key = Platform::KeyId::Unknown;
    std::array<Platform::KeyValue, 2> values{};
    Platform::KeyId functionKey = Platform::KeyId::Unknown;
};

struct Key_Hint
{
    std::array<std::vector<HintId>, 16> byModifiers{};
    std::array<std::vector<HintId>, 16> byFunctionModifiers{};
};

struct Key_Display
{
    HintId hint = NO_HINT;
    std::uint16_t availableCombinations = 0;
    std::uint8_t availableModifiers = 0;
    bool pressed = false;
    bool hovered = false;
    bool functionLayer = false;
    bool functionAvailable = false;
};

struct Tracked_State
{
    TrackedState value{};
    KeyboardPresentation presentation{};

    const TrackedState& displayedState() const
    {
        return presentation.hoverLocked && presentation.initialised ? presentation.tracked : value;
    }

    Platform::InputBatchInfo displayedInput(const Platform::InputBatchInfo& input) const
    {
        return presentation.hoverLocked && presentation.initialised ? presentation.inputInfo()
                                                                    : input;
    }
};

template <> struct ComponentID<Entity_Position>
{
    static constexpr ComponentType VALUE = ComponentType::Entity_Position;
};

template <> struct ComponentID<Entity_Size>
{
    static constexpr ComponentType VALUE = ComponentType::Entity_Size;
};

template <> struct ComponentID<Key_Layout>
{
    static constexpr ComponentType VALUE = ComponentType::Key_Layout;
};

template <> struct ComponentID<Colour>
{
    static constexpr ComponentType VALUE = ComponentType::Colour;
};

template <> struct ComponentID<Key_Values>
{
    static constexpr ComponentType VALUE = ComponentType::Key_Values;
};

template <> struct ComponentID<Key_Hint>
{
    static constexpr ComponentType VALUE = ComponentType::Key_Hint;
};

template <> struct ComponentID<Key_Display>
{
    static constexpr ComponentType VALUE = ComponentType::Key_Display;
};

template <> struct ComponentID<Tracked_State>
{
    static constexpr ComponentType VALUE = ComponentType::Tracked_State;
};
