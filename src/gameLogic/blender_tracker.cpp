// gameLogic/blender_tracker.cpp
#include "gameLogic/blender_tracker.hpp"
#include "gameLogic/blender_bindings.hpp"
#include <string_view>
#include <optional>

namespace
{
    using O = BlenderOperation;
    using C = BlenderContext;

    void Finish(BlenderState& state)
    {
        state.operation = O::None;
        state.context = C::Editor;
        state.menu = BlenderMenuKind::Unknown;
        state.uncertainty = BlenderUncertainty::None;
        state.constraint = BlenderConstraint::None;
        state.axis = 0;
        state.constraintCycle = 0;
        state.numericInput = false;
        state.popupPending = false;
        state.confidence = Confidence::Inferred;
        state.reason = "Operation ended; previous Blender mode / editor inferred";
    }

    void Start(BlenderState& state, O operation)
    {
        state.operation = operation;
        state.constraint = BlenderConstraint::None;
        state.axis = 0;
        state.constraintCycle = 0;
        state.numericInput = false;
    }

    void AssumeEditor(BlenderState& state, const char* reason)
    {
        Finish(state);
        state.confidence = Confidence::Assumed;
        state.reason = reason;
    }

    std::optional<O> ObservedOperation(std::string_view identifier)
    {
        if (identifier == "NONE")
        {
            return O::None;
        }
        // Blender versions may expose either an RNA identifier or a Python operator name.
        const auto matches = [&](std::string_view rna, std::string_view python)
        {
            return identifier == rna || identifier == python;
        };
        if (matches("TRANSFORM_OT_translate", "transform_translate"))
        {
            return O::Move;
        }
        if (matches("NODE_OT_translate_attach", "node_translate_attach") ||
            matches("NODE_OT_translate_attach_remove_on_cancel",
                    "node_translate_attach_remove_on_cancel"))
        {
            return O::Move;
        }
        if (matches("NODE_OT_move_detach_links", "node_move_detach_links") ||
            matches("NODE_OT_move_detach_links_release", "node_move_detach_links_release"))
        {
            return O::NodeDetach;
        }
        if (matches("NODE_OT_link", "node_link"))
        {
            return O::NodeLink;
        }
        if (matches("NODE_OT_links_cut", "node_links_cut"))
        {
            return O::NodeCut;
        }
        if (matches("NODE_OT_add_reroute", "node_add_reroute"))
        {
            return O::NodeReroute;
        }
        if (matches("NODE_OT_links_mute", "node_links_mute"))
        {
            return O::NodeMuteLinks;
        }
        if (matches("NODE_OT_resize", "node_resize"))
        {
            return O::NodeResize;
        }
        if (matches("NODE_OT_select_lasso", "node_select_lasso"))
        {
            return O::NodeLasso;
        }
        if (matches("TRANSFORM_OT_rotate", "transform_rotate"))
        {
            return O::Rotate;
        }
        if (matches("TRANSFORM_OT_trackball", "transform_trackball"))
        {
            return O::Trackball;
        }
        if (matches("TRANSFORM_OT_resize", "transform_resize"))
        {
            return O::Scale;
        }
        if (matches("TRANSFORM_OT_tilt", "transform_tilt"))
        {
            return O::Tilt;
        }
        if (matches("MESH_OT_extrude_region_move", "mesh_extrude_region_move") ||
            matches("MESH_OT_extrude_context_move", "mesh_extrude_context_move") ||
            matches("MESH_OT_extrude_vertices_move", "mesh_extrude_vertices_move") ||
            matches("MESH_OT_extrude_edges_move", "mesh_extrude_edges_move") ||
            matches("MESH_OT_extrude_faces_move", "mesh_extrude_faces_move") ||
            matches("CURVE_OT_extrude_move", "curve_extrude_move"))
        {
            return O::Extrude;
        }
        if (matches("MESH_OT_duplicate_move", "mesh_duplicate_move") ||
            matches("OBJECT_OT_duplicate_move", "object_duplicate_move") ||
            matches("OBJECT_OT_duplicate_move_linked", "object_duplicate_move_linked") ||
            matches("CURVE_OT_duplicate_move", "curve_duplicate_move") ||
            matches("NODE_OT_duplicate_move", "node_duplicate_move") ||
            matches("NODE_OT_duplicate_move_linked", "node_duplicate_move_linked") ||
            matches("NODE_OT_duplicate_move_keep_inputs", "node_duplicate_move_keep_inputs"))
        {
            return O::Duplicate;
        }
        if (matches("TRANSFORM_OT_edge_slide", "transform_edge_slide") ||
            matches("TRANSFORM_OT_vert_slide", "transform_vert_slide"))
        {
            return O::EdgeSlide;
        }
        if (matches("MESH_OT_inset", "mesh_inset"))
        {
            return O::Inset;
        }
        if (matches("MESH_OT_bevel", "mesh_bevel"))
        {
            return O::Bevel;
        }
        if (matches("MESH_OT_knife_tool", "mesh_knife_tool"))
        {
            return O::Knife;
        }
        if (matches("MESH_OT_loopcut", "mesh_loopcut") ||
            matches("MESH_OT_loopcut_slide", "mesh_loopcut_slide"))
        {
            return O::LoopCut;
        }
        if (matches("VIEW3D_OT_select_box", "view3d_select_box") ||
            matches("NODE_OT_select_box", "node_select_box"))
        {
            return O::BoxSelect;
        }
        if (matches("VIEW3D_OT_select_circle", "view3d_select_circle") ||
            matches("NODE_OT_select_circle", "node_select_circle"))
        {
            return O::CircleSelect;
        }
        return std::nullopt;
    }

    bool NodeMouseInput(BlenderState& state, const Platform::InputEvent& event)
    {
        using namespace Platform;
        if (IsBlenderNodeMouseTool(state.operation))
        {
            const bool right_drag = state.operation == O::NodeCut ||
                                    state.operation == O::NodeReroute ||
                                    state.operation == O::NodeMuteLinks;
            if ((!event.mousePressed &&
                 event.mouseButton == (right_drag ? MouseButton::Right : MouseButton::Left)) ||
                (event.mousePressed && event.mouseButton == MouseButton::Right && !right_drag))
            {
                Finish(state);
            }
            return true;
        }
        if (!event.mousePressed && event.mouseButton == MouseButton::Left &&
            IsBlenderNodeTransform(state.operation))
        {
            Finish(state);
            return true;
        }
        if (state.operation != O::None || !event.mousePressed)
        {
            return false;
        }
        auto modifiers = event.modifiers;
        if (state.macCommandAliases && (modifiers & COMMAND) && !(modifiers & CONTROL))
        {
            modifiers = (modifiers & ~COMMAND) | CONTROL;
        }
        if (event.mouseButton == MouseButton::Right)
        {
            if (modifiers == SHIFT)
            {
                Start(state, O::NodeReroute);
            }
            else if (modifiers == CONTROL)
            {
                Start(state, O::NodeCut);
            }
            else if (modifiers == (CONTROL | OPTION))
            {
                Start(state, O::NodeMuteLinks);
            }
            else if (modifiers == 0)
            {
                state.context = C::Menu;
                state.menu = BlenderMenuKind::PreserveMode;
                state.popupPending = true;
            }
            else
            {
                return false;
            }
            state.confidence = Confidence::Inferred;
            state.reason = "Node mouse gesture / context menu inferred; fresh Blender reports "
                           "confirm running tools";
            return true;
        }
        if (event.mouseButton == MouseButton::Left &&
            (modifiers == (CONTROL | OPTION) || modifiers == (CONTROL | OPTION | SHIFT)))
        {
            Start(state, O::NodeLasso);
            state.confidence = Confidence::Inferred;
            state.reason =
                "Node lasso inferred from mouse modifiers; fresh Blender reports confirm the tool";
            return true;
        }
        // Plain clicks may select a node, socket or property. Do not guess a socket drag here.
        return false;
    }

    void MouseInput(BlenderState& state, const Platform::InputEvent& event)
    {
        using B = Platform::MouseButton;
        if (event.mouseButton == B::Unknown || event.mouseButton == B::Other)
        {
            if (event.mousePressed)
            {
                InvalidateBlenderState(
                    state,
                    "Unrecognized mouse action; check Blender context and select KeyGuide → Resync Mode");
            }
            return;
        }
        if (state.context != C::Editor)
        {
            if (!event.mousePressed)
            {
                return;
            }
            if (event.mouseButton == B::Right)
            {
                Finish(state);
            }
            else if (state.context == C::DeleteConfirm && event.mouseButton == B::Left)
            {
                Finish(state);
            }
            else if (state.context == C::Menu && event.mouseButton == B::Left)
            {
                if (state.menu == BlenderMenuKind::PreserveMode)
                {
                    AssumeEditor(state, "Menu choice preserves the assumed base mode");
                }
                else
                {
                    InvalidateBlenderState(state, "Unknown menu choice; check Blender mode and "
                                                  "select KeyGuide → Resync Mode");
                }
            }
            else if (state.context == C::Search && event.mouseButton == B::Left)
            {
                state.popupPending = false;
                InvalidateBlenderState(state, "Search selection is unknown; check Blender mode "
                                              "and select KeyGuide → Resync Mode");
            }
            // Clicking within Rename and middle-button popup interaction do not establish a result.
            return;
        }
        if (state.editor == BlenderEditor::Nodes && NodeMouseInput(state, event))
        {
            return;
        }
        if (state.operation == O::BoxSelect)
        {
            if (event.mouseButton == B::Right || !event.mousePressed)
            {
                Finish(state);
            }
            return;
        }
        if (!event.mousePressed)
        {
            return;
        }
        if (state.operation == O::Knife)
        {
            return; // Left adds cuts; right starts a new cut. Neither exits Knife.
        }
        if (state.operation == O::CircleSelect)
        {
            if (event.mouseButton == B::Right)
            {
                Finish(state);
            }
            return;
        }
        if (state.operation == O::LoopCut)
        {
            if (event.mouseButton == B::Left)
            {
                Start(state, O::EdgeSlide);
            }
            else if (event.mouseButton == B::Right)
            {
                Finish(state);
            }
            return;
        }
        if (state.operation != O::None)
        {
            if (event.mouseButton == B::Left || event.mouseButton == B::Right)
            {
                Finish(state);
            }
            // Middle-button transform constraints depend on mouse motion, which we do not infer.
            else if (IsBlenderTransform(state.operation))
            {
                state.axis = 0;
                state.constraintCycle = 0;
                state.constraint = BlenderConstraint::None;
                state.reason = "Mouse-selected transform constraint is unknown; keyboard axis keys "
                               "remain available";
            }
            return;
        }
        if (event.mouseButton == B::Right)
        {
            const bool add_point =
                state.mode == BlenderMode::CurveEdit &&
                (event.modifiers == Platform::CONTROL ||
                 (state.macCommandAliases && event.modifiers == Platform::COMMAND));
            if (add_point)
            {
                state.confidence = Confidence::Inferred;
                state.reason = "Ctrl / Command right click adds a curve control point";
                return;
            }
            state.context = C::Menu;
            state.menu = BlenderMenuKind::Unknown;
            state.popupPending = true;
        }
        else
        {
            // Ordinary viewport selection/navigation is retained under the manually assumed
            // context.
            state.confidence = Confidence::Assumed;
            state.reason =
                "Editor selection / navigation assumed; context changes need KeyGuide → Resync Mode";
        }
    }
} // namespace

BlenderEditor ClassifyBlenderEditor(std::string_view identifier)
{
    if (identifier == "VIEW_3D")
    {
        return BlenderEditor::Viewport;
    }
    if (identifier == "NODE_EDITOR" || identifier == "GeometryNodeTree" ||
        identifier == "ShaderNodeTree" || identifier == "CompositorNodeTree" ||
        identifier == "TextureNodeTree")
    {
        return BlenderEditor::Nodes;
    }
    return BlenderEditor::Unsupported;
}

const char* BlenderBaseStateName(const BlenderState& state)
{
    if (state.editor == BlenderEditor::Nodes)
    {
        const std::string_view identifier(state.observation.editor.data());
        if (identifier == "GeometryNodeTree")
        {
            return "GEOMETRY NODES";
        }
        if (identifier == "ShaderNodeTree")
        {
            return "SHADER NODES";
        }
        if (identifier == "CompositorNodeTree")
        {
            return "COMPOSITOR";
        }
        return "NODES";
    }
    return BlenderModeName(state.mode);
}

void ObserveBlenderState(BlenderState& state, const Platform::BlenderObservation& observation)
{
    if (state.observation == observation)
    {
        return;
    }
    const bool was_connected = state.observation.connected;
    const auto previous_editor = state.observation.editor;
    state.observation = observation;
    if (!observation.connected)
    {
        if (was_connected)
        {
            InvalidateBlenderState(state, "Blender state bridge disconnected; waiting for a fresh "
                                          "report, or select KeyGuide → Resync Mode");
        }
        return;
    }
    ++state.revision;
    if (!observation.current)
    {
        return; // A report made before the latest input cannot undo a newer inferred transition.
    }
    const std::string_view mode(observation.mode.data()), editor(observation.editor.data());
    const auto actual_editor = ClassifyBlenderEditor(editor);
    if (mode == "UNKNOWN" && actual_editor != BlenderEditor::Nodes)
    {
        InvalidateBlenderState(state, "Blender has no readable window context; waiting for state");
        return;
    }
    const auto actual_mode = mode == "OBJECT"       ? BlenderMode::Object
                             : mode == "EDIT_MESH"  ? BlenderMode::MeshEdit
                             : mode == "EDIT_CURVE" ? BlenderMode::CurveEdit
                                                    : BlenderMode::Unsupported;
    const bool mode_changed = state.mode != actual_mode;
    const bool editor_changed =
        state.editor != actual_editor || (was_connected && previous_editor != observation.editor);
    state.mode = actual_mode;
    state.editor = actual_editor;
    if (actual_mode == BlenderMode::MeshEdit || actual_mode == BlenderMode::CurveEdit)
    {
        state.lastEditMode = actual_mode;
    }
    if (observation.selectionMask)
    {
        state.selectionMask = observation.selectionMask;
    }
    const bool supported =
        actual_editor == BlenderEditor::Nodes ||
        (actual_editor == BlenderEditor::Viewport && actual_mode != BlenderMode::Unsupported);
    if (editor_changed || (mode_changed && actual_editor == BlenderEditor::Viewport) || !supported)
    {
        Finish(state);
    }
    if (observation.interfaceLocked)
    {
        InvalidateBlenderState(state, "Blender interface is busy; waiting for it to unlock",
                               BlenderUncertainty::Tool);
        return;
    }
    if (!supported)
    {
        state.confidence = Confidence::Observed;
        state.reason = "Real Blender mode / editor observed; no cheat sheet for this context yet";
        return;
    }
    if (!observation.modalAvailable)
    {
        if (state.confidence == Confidence::Uncertain)
        {
            state.uncertainty = BlenderUncertainty::Tool;
            state.reason = "Real Blender base mode observed; this Blender version cannot report "
                           "running tools. Escape clears the remaining inferred tool / popup";
        }
        else if (state.operation == O::None && state.context == C::Editor)
        {
            state.confidence = Confidence::Observed;
            state.reason = "Real Blender base mode observed; temporary tools are inferred";
        }
        return;
    }
    // The existing bridge sends operator IDs, not transform mode properties. Radius uses
    // Blender's generic transform operator; its subtype can only be inferred after Alt-S.
    const std::string_view identifier(observation.operation.data());
    const bool inferred_radius =
        actual_editor == BlenderEditor::Viewport && actual_mode == BlenderMode::CurveEdit &&
        state.operation == O::Radius &&
        (identifier == "TRANSFORM_OT_transform" || identifier == "transform_transform");
    const auto operation =
        inferred_radius ? std::optional<O>(O::Radius) : ObservedOperation(identifier);
    if (!operation)
    {
        state.popupPending = false;
        InvalidateBlenderState(state,
                               "An unmodeled Blender tool is running; ending it will "
                               "automatically restore the observed base mode",
                               BlenderUncertainty::Tool);
        return;
    }
    if (*operation == O::None)
    {
        if (!state.popupPending)
        {
            Finish(state);
            state.confidence = Confidence::Observed;
            state.reason = "Mode, editor and absence of running tools read from Blender";
        }
        else
        {
            state.reason = "Real base mode observed; popup completion is still inferred. "
                           "Escape closes it and resyncs";
            if (state.confidence == Confidence::Uncertain)
            {
                state.uncertainty = BlenderUncertainty::Tool;
            }
        }
        return;
    }
    const bool preserve_move = *operation == O::Move &&
                               (state.operation == O::Extrude || state.operation == O::Duplicate ||
                                state.operation == O::NodeDetach);
    const bool preserve_slide = *operation == O::LoopCut && state.operation == O::EdgeSlide;
    if (!preserve_move && !preserve_slide && state.operation != *operation)
    {
        Start(state, *operation);
        state.numericInput = false;
    }
    state.context = C::Editor;
    state.popupPending = false;
    state.uncertainty = BlenderUncertainty::None;
    state.confidence = inferred_radius ? Confidence::Inferred : Confidence::Observed;
    state.reason =
        inferred_radius
            ? "Generic transform observed; curve radius inferred from Alt-S"
            : "Real Blender mode and running tool observed; axis / numeric input inferred";
}

const char* BlenderModeName(BlenderMode mode)
{
    switch (mode)
    {
        case BlenderMode::Object:
            return "OBJECT";
        case BlenderMode::MeshEdit:
            return "MESH EDIT";
        case BlenderMode::CurveEdit:
            return "CURVE EDIT";
        case BlenderMode::Unsupported:
            return "OTHER MODE";
    }
    return "OTHER MODE";
}

const char* BlenderOperationName(BlenderOperation operation)
{
    switch (operation)
    {
        case O::None:
            return "";
        case O::Move:
            return "MOVE";
        case O::Rotate:
            return "ROTATE";
        case O::Trackball:
            return "TRACKBALL";
        case O::Scale:
            return "SCALE";
        case O::Tilt:
            return "TILT";
        case O::Radius:
            return "RADIUS";
        case O::Extrude:
            return "EXTRUDE";
        case O::Duplicate:
            return "DUPLICATE";
        case O::Inset:
            return "INSET";
        case O::Bevel:
            return "BEVEL";
        case O::Knife:
            return "KNIFE";
        case O::LoopCut:
            return "LOOP CUT";
        case O::EdgeSlide:
            return "EDGE SLIDE";
        case O::BoxSelect:
            return "BOX SELECT";
        case O::CircleSelect:
            return "CIRCLE SELECT";
        case O::NodeLink:
            return "LINK";
        case O::NodeCut:
            return "CUT LINKS";
        case O::NodeReroute:
            return "REROUTE";
        case O::NodeMuteLinks:
            return "MUTE LINKS";
        case O::NodeResize:
            return "NODE WIDTH";
        case O::NodeLasso:
            return "LASSO";
        case O::NodeDetach:
            return "DETACH";
    }
    return "";
}

void ResetBlenderState(BlenderState& state, BlenderMode mode, const char* reason)
{
    const bool aliases = state.macCommandAliases;
    const auto edit_mode =
        mode == BlenderMode::CurveEdit || mode == BlenderMode::MeshEdit ? mode : state.lastEditMode;
    const auto revision = state.revision + 1;
    state = {};
    state.mode = mode;
    state.lastEditMode = edit_mode;
    state.macCommandAliases = aliases;
    state.reason = reason;
    state.revision = revision;
}

void InvalidateBlenderState(BlenderState& state, const char* reason, BlenderUncertainty uncertainty)
{
    // A later tool interruption must never turn an unknown base mode into a recoverable one.
    if (state.confidence == Confidence::Uncertain && state.uncertainty == BlenderUncertainty::Mode)
    {
        uncertainty = BlenderUncertainty::Mode;
    }
    const bool popup = state.popupPending;
    Finish(state);
    state.popupPending = popup;
    state.confidence = Confidence::Uncertain;
    state.uncertainty = uncertainty;
    state.reason = reason;
    ++state.revision;
}

void TrackBlenderInput(BlenderState& state, const Platform::InputEvent& event)
{
    using namespace Platform;
    state.precision = (event.modifiers & SHIFT) != 0;
    state.snapInverted = (event.modifiers & CONTROL) != 0;
    if (event.type == EventType::NodeEditorBaseline)
    {
        ResetBlenderState(state, state.mode,
                          event.baselineReason
                              ? event.baselineReason
                              : "Node Editor assumed; manual baseline changes only the overlay");
        state.editor = BlenderEditor::Nodes;
        return;
    }
    if (event.type == EventType::ObjectBaseline || event.type == EventType::MeshEditBaseline ||
        event.type == EventType::CurveEditBaseline)
    {
        ResetBlenderState(
            state,
            event.type == EventType::ObjectBaseline      ? BlenderMode::Object
            : event.type == EventType::CurveEditBaseline ? BlenderMode::CurveEdit
                                                         : BlenderMode::MeshEdit,
            event.baselineReason
                ? event.baselineReason
                : "Manual Blender mode / 3D Viewport baseline; check actual mode in Blender");
        return;
    }
    if (event.type == EventType::Modifiers || event.type == EventType::KeyUp ||
        ModifierBit(event.key))
    {
        return;
    }
    if (event.type >= EventType::NormalBaseline)
    {
        return; // Vim baselines cannot reset Blender.
    }
    if (state.observation.connected &&
        ((state.mode == BlenderMode::Unsupported && state.editor == BlenderEditor::Viewport) ||
         ClassifyBlenderEditor(state.observation.editor.data()) == BlenderEditor::Unsupported ||
         state.observation.interfaceLocked))
    {
        return; // Physical feedback continues; viewport hints do not apply to this context.
    }
    // Blender never receives the macOS application switch or other default system shortcuts.
    const bool system_shortcut =
        (event.key == KeyId::Tab && event.modifiers == COMMAND) ||
        (state.macCommandAliases && MacSystemShortcut(event.key, event.modifiers));
    if (state.confidence == Confidence::Uncertain)
    {
        if (state.uncertainty == BlenderUncertainty::Tool ||
            (state.observation.connected && state.observation.current))
        {
            if (event.type == EventType::KeyDown && event.key == KeyId::Escape)
            {
                if (!event.repeat)
                {
                    AssumeEditor(
                        state,
                        "Escape canceled the interrupted tool; previous mode / editor assumed");
                    ++state.revision;
                }
            }
            else if (!system_shortcut &&
                     (event.type == EventType::KeyDown || event.type == EventType::Unsupported ||
                      (event.type == EventType::MouseAction && event.mousePressed)))
            {
                InvalidateBlenderState(state, "Further input while the tool was unknown; check "
                                              "Blender mode and select KeyGuide → Resync Mode");
            }
        }
        return; // Escape cannot establish an unknown base mode.
    }
    if (event.type == EventType::Unsupported)
    {
        InvalidateBlenderState(
            state, "Unsupported input; check Blender mode / editor and choose KeyGuide → Resync Mode");
        return;
    }
    ++state.revision;
    if (event.type == EventType::MouseAction)
    {
        MouseInput(state, event);
        return;
    }
    if (event.type != EventType::KeyDown)
    {
        return;
    }
    if (system_shortcut)
    {
        return;
    }
    if (event.key == KeyId::Escape)
    {
        if (!event.repeat && (state.operation != O::None || state.context != C::Editor))
        {
            Finish(state);
        }
        return;
    }
    if (state.context == C::Search || state.context == C::Rename)
    {
        if (event.key == KeyId::Enter || event.key == KeyId::NumpadEnter)
        {
            if (state.context == C::Rename)
            {
                Finish(state);
            }
            else
            {
                state.popupPending = false;
                InvalidateBlenderState(state, "Search result executed; resulting mode / operation "
                                              "is unknown; use KeyGuide → Resync Mode");
            }
        }
        // Never retain rename/search text, paths, numeric values or scene data.
        return;
    }
    if (state.context == C::Menu && state.menu == BlenderMenuKind::PreserveMode &&
        event.modifiers == 0)
    {
        if (event.key >= KeyId::One && event.key <= KeyId::Nine && !event.repeat)
        {
            AssumeEditor(state, "Numbered menu choice preserves the assumed base mode");
            return;
        }
        if ((event.key >= KeyId::A && event.key <= KeyId::Z) || event.key == KeyId::Backspace ||
            event.key == KeyId::Space)
        {
            return; // Menu filtering/accelerators are not retained as text or guessed as commands.
        }
    }
    if (state.operation == O::LoopCut &&
        (event.key == KeyId::Enter || event.key == KeyId::NumpadEnter))
    {
        if (!event.repeat)
        {
            Start(state, O::EdgeSlide);
        }
        return;
    }
    const auto id = FindBlenderBinding(state, event.key, event.modifiers);
    if (id == NO_HINT)
    {
        if (state.context == C::Editor && state.operation == O::None && event.modifiers == 0 &&
            ((event.key >= KeyId::F5 && event.key <= KeyId::F8) || event.key == KeyId::F10))
        {
            state.confidence = Confidence::Assumed;
            state.reason = "Unassigned default function key; previous base mode retained";
            return;
        }
        const bool unlisted_tool_key = state.operation != O::None && event.key >= KeyId::Space &&
                                       event.key <= KeyId::Grave &&
                                       !(event.modifiers & (CONTROL | COMMAND));
        InvalidateBlenderState(
            state, "Unsupported shortcut / tool input; check Blender context and use KeyGuide → Resync Mode",
            unlisted_tool_key ? BlenderUncertainty::Tool : BlenderUncertainty::Mode);
        return;
    }
    const auto& binding = BlenderBindings()[id];
    using A = BlenderAction;
    if (event.repeat && binding.action != A::Complete && binding.action != A::Numeric)
    {
        return;
    }
    state.confidence = Confidence::Inferred;
    state.reason = "Inferred from default Blender shortcuts in the assumed editor";
    switch (binding.action)
    {
        case A::ToggleMode:
            state.mode =
                state.mode == BlenderMode::Object ? state.lastEditMode : BlenderMode::Object;
            break;
        case A::Move:
            if (state.editor == BlenderEditor::Viewport && state.mode == BlenderMode::MeshEdit &&
                state.operation == O::Move)
            {
                Start(state, O::EdgeSlide);
            }
            else
            {
                Start(state, O::Move);
            }
            break;
        case A::Rotate:
            Start(state, state.editor == BlenderEditor::Viewport && state.operation == O::Rotate
                             ? O::Trackball
                             : O::Rotate);
            break;
        case A::Scale:
            Start(state, O::Scale);
            break;
        case A::Tilt:
            Start(state, O::Tilt);
            break;
        case A::Radius:
            Start(state, O::Radius);
            break;
        case A::Extrude:
            Start(state, O::Extrude);
            break;
        case A::Duplicate:
            Start(state, O::Duplicate);
            break;
        case A::Inset:
            Start(state, O::Inset);
            break;
        case A::Bevel:
            Start(state, O::Bevel);
            break;
        case A::Knife:
            Start(state, O::Knife);
            break;
        case A::LoopCut:
            Start(state, O::LoopCut);
            break;
        case A::BoxSelect:
            Start(state, O::BoxSelect);
            break;
        case A::CircleSelect:
            Start(state, O::CircleSelect);
            break;
        case A::Menu:
            state.context = C::Menu;
            state.menu = binding.menu;
            state.popupPending = true;
            break;
        case A::Search:
            state.context = C::Search;
            state.popupPending = true;
            break;
        case A::Rename:
            state.context = C::Rename;
            state.popupPending = true;
            break;
        case A::DeleteConfirm:
            state.context = C::DeleteConfirm;
            state.popupPending = true;
            break;
        case A::ExecuteUnknown:
            if (state.context == C::Menu && state.menu == BlenderMenuKind::PreserveMode)
            {
                AssumeEditor(state, "Menu choice preserves the assumed base mode");
            }
            else
            {
                if (event.key == KeyId::Enter || event.key == KeyId::NumpadEnter)
                {
                    state.popupPending = false;
                }
                InvalidateBlenderState(state, "Command may change context or start a tool; check "
                                              "actual mode and use KeyGuide → Resync Mode");
            }
            break;
        case A::Axis:
        {
            const char axis = static_cast<char>(event.key);
            const auto constraint =
                event.modifiers & SHIFT ? BlenderConstraint::Plane : BlenderConstraint::Axis;
            const bool same = state.axis == axis && state.constraint == constraint;
            state.constraintCycle =
                same
                    ? static_cast<std::uint8_t>(
                          (state.constraintCycle + 1) %
                          (state.operation == O::Knife || state.editor == BlenderEditor::Nodes ? 2
                                                                                               : 3))
                    : 1;
            state.axis = state.constraintCycle ? axis : 0;
            state.constraint = state.constraintCycle ? constraint : BlenderConstraint::None;
            break;
        }
        case A::ClearConstraint:
            state.axis = 0;
            state.constraintCycle = 0;
            state.constraint = BlenderConstraint::None;
            break;
        case A::Numeric:
            state.numericInput = true;
            break;
        case A::Confirm:
        case A::Cancel:
            Finish(state);
            break;
        case A::SelectVertices:
        case A::SelectEdges:
        case A::SelectFaces:
        {
            const std::uint8_t bit = binding.action == A::SelectVertices ? 1
                                     : binding.action == A::SelectEdges  ? 2
                                                                         : 4;
            if (event.modifiers & SHIFT)
            {
                const auto mask = state.selectionMask ^ bit;
                if (mask)
                {
                    state.selectionMask = mask;
                }
            }
            else
            {
                state.selectionMask = bit;
            }
            break;
        }
        case A::Complete:
            break;
        case A::ShowViewport:
            // Switching an area's editor keeps the object mode.
            state.editor = BlenderEditor::Viewport;
            state.confidence = Confidence::Assumed;
            state.reason = "3D Viewport shown; previous mode assumed unchanged";
            break;
        case A::PreserveMode:
            state.confidence = Confidence::Assumed;
            state.reason = "Shortcut observed; previous mode / editor assumed unchanged";
            break;
    }
}
