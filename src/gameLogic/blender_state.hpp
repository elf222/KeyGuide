// gameLogic/blender_state.hpp
#pragma once
#include <cstdint>
#include <string_view>
#include "gameLogic/tracking_types.hpp"
#include "platform/blender_observation.hpp"

enum class BlenderMode
{
    Object,
    MeshEdit,
    CurveEdit,
    Unsupported
};
enum class BlenderEditor
{
    Viewport,
    Nodes,
    Unsupported
};
enum class BlenderOperation
{
    None,
    Move,
    Rotate,
    Trackball,
    Scale,
    Tilt,
    Radius,
    Extrude,
    Duplicate,
    Inset,
    Bevel,
    Knife,
    LoopCut,
    EdgeSlide,
    BoxSelect,
    CircleSelect,
    NodeLink,
    NodeCut,
    NodeReroute,
    NodeMuteLinks,
    NodeResize,
    NodeLasso,
    NodeDetach
};
enum class BlenderContext
{
    Editor,
    Menu,
    Search,
    Rename,
    DeleteConfirm
};
enum class BlenderConstraint
{
    None,
    Axis,
    Plane
};
enum class BlenderMenuKind
{
    Unknown,
    PreserveMode
};
enum class BlenderUncertainty
{
    None,
    Tool,
    Mode
};

struct BlenderState
{
    BlenderMode mode = BlenderMode::Object;
    BlenderEditor editor = BlenderEditor::Viewport;
    BlenderMode lastEditMode = BlenderMode::MeshEdit;
    BlenderOperation operation = BlenderOperation::None;
    BlenderContext context = BlenderContext::Editor;
    BlenderConstraint constraint = BlenderConstraint::None;
    BlenderMenuKind menu = BlenderMenuKind::Unknown;
    BlenderUncertainty uncertainty = BlenderUncertainty::None;
    Confidence confidence = Confidence::Assumed;
    char axis = 0;
    std::uint8_t constraintCycle = 0, selectionMask = 1;
    bool numericInput = false, precision = false, snapInverted = false;
    bool macCommandAliases = false;
    bool popupPending = false;
    Platform::BlenderObservation observation{};
    const char* reason = "Object Mode / 3D Viewport assumed; use KeyGuide → Resync Mode to select the actual edit mode";
    std::uint64_t revision = 0;
};

const char* BlenderModeName(BlenderMode mode);
const char* BlenderOperationName(BlenderOperation operation);
BlenderEditor ClassifyBlenderEditor(std::string_view identifier);
const char* BlenderBaseStateName(const BlenderState& state);
