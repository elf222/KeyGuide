// gameLogic/blender_bindings.hpp
#pragma once
#include <span>
#include <string_view>
#include "gameLogic/blender_state.hpp"
#include "platform/input_event.hpp"

enum class BlenderScope
{
    Editor,
    Idle,
    Object,
    Mesh,
    ObjectOrMesh,
    Curve,
    Node,
    NodeTransform,
    NodeMove,
    NodeLink,
    NodeResize,
    Compositor,
    Transform,
    TransformInput,
    Modal,
    Knife,
    Bevel,
    Inset,
    LoopCut,
    BoxSelect,
    CircleSelect,
    Menu,
    Text,
    DeleteConfirm
};
enum class BlenderAction
{
    Complete,
    PreserveMode,
    ToggleMode,
    Move,
    Rotate,
    Scale,
    Tilt,
    Radius,
    Extrude,
    Duplicate,
    Inset,
    Bevel,
    Knife,
    LoopCut,
    BoxSelect,
    CircleSelect,
    Menu,
    Search,
    Rename,
    DeleteConfirm,
    ExecuteUnknown,
    ShowViewport,
    Axis,
    ClearConstraint,
    Numeric,
    Confirm,
    Cancel,
    SelectVertices,
    SelectEdges,
    SelectFaces
};

struct BlenderBinding
{
    Platform::KeyId key = Platform::KeyId::Unknown;
    std::uint8_t modifiers = 0;
    BlenderScope scope = BlenderScope::Idle;
    BlenderAction action = BlenderAction::Complete;
    std::string_view shortHint, fullHint;
    char32_t mnemonic = 0;
    bool anyModifiers = false, macOnly = false;
    BlenderMenuKind menu = BlenderMenuKind::Unknown;
};

std::span<const BlenderBinding> BlenderBindings();
bool IsBlenderTransform(BlenderOperation operation);
bool IsBlenderNodeTransform(BlenderOperation operation);
bool IsBlenderNodeMouseTool(BlenderOperation operation);
bool MacSystemShortcut(Platform::KeyId key, std::uint8_t modifiers);
bool BlenderBindingAvailable(const BlenderBinding& binding, const BlenderState& state);
HintId FindBlenderBinding(const BlenderState& state, Platform::KeyId key, std::uint8_t modifiers);
