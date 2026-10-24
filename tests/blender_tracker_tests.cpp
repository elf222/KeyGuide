// tests/blender_tracker_tests.cpp
#include "test_helpers.hpp"
#include <utility>
#include "gameLogic/blender_bindings.hpp"
#include "gameLogic/blender_tracker.hpp"
#include "gameLogic/keyboard_status.hpp"
#include "gameLogic/tracker.hpp"
#include "platform/blender_protocol.hpp"

namespace
{
    Platform::BlenderObservation Observation(std::string_view mode = "EDIT_MESH",
                                             std::string_view operation = "NONE",
                                             std::string_view editor = "VIEW_3D")
    {
        const auto packet =
            "KC_STATE 1 token 123 session 1 1 " + std::string(mode) + " " + std::string(editor) +
            " 3 " + (operation == "UNAVAILABLE" ? "0 " : "1 ") + std::string(operation) + " 0";
        const auto decoded = Platform::ParseBlenderMessage(packet);
        CHECK(decoded.has_value());
        auto observation = decoded->observation;
        observation.current = true;
        return observation;
    }

    void TestNodeEditing()
    {
        using namespace Platform;
        using O = BlenderOperation;
        TrackedState tracked;
        tracked.appProfile = AppProfile::Blender;
        auto& state = tracked.blender;
        std::uint64_t sequence = 0;
        const auto observe = [&](std::string_view operation = "NONE",
                                 std::string_view editor = "GeometryNodeTree",
                                 std::string_view mode = "EDIT_MESH")
        {
            auto report = Observation(mode, operation, editor);
            report.inputSequence = ++sequence;
            ObserveBlenderState(state, report);
        };
        const auto key = [&](KeyId id, std::uint8_t modifiers = 0)
        {
            TrackInput(tracked, {.key = id, .modifiers = modifiers});
        };
        const auto mouse = [&](MouseButton button, std::uint8_t modifiers = 0, bool pressed = true)
        {
            TrackInput(tracked, {.type = EventType::MouseAction,
                                 .modifiers = modifiers,
                                 .mouseButton = button,
                                 .mousePressed = pressed});
        };
        const auto hint = [&](KeyId id, std::uint8_t modifiers = 0)
        {
            return FindBlenderBinding(state, id, modifiers);
        };
        InputBatchInfo input{
            .foreground = true, .available = true, .appProfile = AppProfile::Blender};
        observe();
        CHECK(state.editor == BlenderEditor::Nodes && state.mode == BlenderMode::MeshEdit);
        CHECK(BuildKeyboardStatus(tracked, input).label == "GEOMETRY NODES");
        CHECK(hint(KeyId::E) == NO_HINT && hint(KeyId::K) == NO_HINT &&
              hint(KeyId::One) == NO_HINT);
        CHECK(BlenderBindings()[hint(KeyId::Shift)].fullHint.find("reroute") !=
              std::string_view::npos);
        CHECK(hint(KeyId::Control, CONTROL) != NO_HINT && hint(KeyId::Control, SHIFT) == NO_HINT);
        CHECK(hint(KeyId::Option, CONTROL | OPTION) != NO_HINT);
        // Every physical chord has one active hint, avoiding tracker / renderer disagreement.
        for (std::size_t raw_key = 0; raw_key < KEY_COUNT; ++raw_key)
        {
            for (std::uint8_t modifiers = 0; modifiers < 16; ++modifiers)
            {
                std::size_t count = 0;
                for (const auto& binding : BlenderBindings())
                {
                    count += binding.key == static_cast<KeyId>(raw_key) &&
                             (binding.anyModifiers || binding.modifiers == modifiers) &&
                             BlenderBindingAvailable(binding, state);
                }
                CHECK(count <= 1);
            }
        }
        key(KeyId::Tab);
        key(KeyId::Tab, CONTROL);
        CHECK(state.editor == BlenderEditor::Nodes && state.mode == BlenderMode::MeshEdit);
        for (const auto [id, modifiers] : {std::pair{KeyId::J, 0},
                                           {KeyId::J, SHIFT},
                                           {KeyId::J, CONTROL},
                                           {KeyId::X, 0},
                                           {KeyId::X, CONTROL},
                                           {KeyId::M, 0},
                                           {KeyId::H, 0},
                                           {KeyId::H, SHIFT},
                                           {KeyId::L, 0},
                                           {KeyId::L, SHIFT},
                                           {KeyId::G, CONTROL},
                                           {KeyId::G, CONTROL | OPTION},
                                           {KeyId::Tab, SHIFT}})
        {
            key(id, modifiers);
            CHECK(state.confidence != Confidence::Uncertain && state.operation == O::None);
        }
        key(KeyId::A, SHIFT);
        observe();
        CHECK(state.context == BlenderContext::Menu && state.popupPending);
        mouse(MouseButton::Left);
        observe("NODE_OT_translate_attach");
        CHECK(state.operation == O::Move && state.context == BlenderContext::Editor);
        key(KeyId::X);
        key(KeyId::X);
        CHECK(!state.axis); // Node layouts have no local 3D axis orientation.
        CHECK(hint(KeyId::Z) == NO_HINT);
        key(KeyId::Two);
        key(KeyId::F);
        key(KeyId::T);
        CHECK(state.numericInput && state.confidence != Confidence::Uncertain);
        key(KeyId::Enter);
        observe();
        key(KeyId::D, OPTION);
        CHECK(state.operation == O::Duplicate); // Alt-D duplicates linked groups, not detaches.
        observe("node_duplicate_move_linked");
        observe("TRANSFORM_OT_translate");
        CHECK(state.operation == O::Duplicate);
        key(KeyId::Escape);
        observe();
        key(KeyId::G);
        key(KeyId::G);
        CHECK(state.operation ==
              O::Move); // Mesh scene mode must not invoke mesh edge slide in nodes.
        key(KeyId::R);
        key(KeyId::R);
        CHECK(state.operation == O::Rotate && hint(KeyId::X) == NO_HINT);
        key(KeyId::Escape);
        observe();
        for (const auto [modifiers, operation] :
             {std::pair{SHIFT, O::NodeReroute},
              {CONTROL, O::NodeCut},
              {static_cast<std::uint8_t>(CONTROL | OPTION), O::NodeMuteLinks}})
        {
            mouse(MouseButton::Right, modifiers);
            CHECK(state.operation == operation && !state.popupPending);
            mouse(MouseButton::Right, modifiers, false);
            CHECK(state.operation == O::None && state.confidence != Confidence::Uncertain);
        }
        for (const auto [identifier, operation] : {std::pair{"NODE_OT_add_reroute", O::NodeReroute},
                                                   {"node_links_cut", O::NodeCut},
                                                   {"NODE_OT_links_mute", O::NodeMuteLinks},
                                                   {"NODE_OT_link", O::NodeLink},
                                                   {"NODE_OT_resize", O::NodeResize},
                                                   {"NODE_OT_select_lasso", O::NodeLasso},
                                                   {"NODE_OT_move_detach_links", O::NodeDetach}})
        {
            observe(identifier);
            CHECK(state.operation == operation && state.confidence == Confidence::Observed);
            CHECK(hint(KeyId::Escape) != NO_HINT);
            key(KeyId::Escape);
            observe();
            CHECK(state.operation == O::None && hint(KeyId::G) != NO_HINT);
        }
        observe("NODE_OT_link");
        CHECK(hint(KeyId::Option) != NO_HINT && hint(KeyId::Enter) == NO_HINT);
        mouse(MouseButton::Left, 0, false);
        CHECK(state.operation == O::None);
        observe("NODE_OT_resize");
        CHECK(hint(KeyId::Control) != NO_HINT && hint(KeyId::One) == NO_HINT);
        mouse(MouseButton::Right);
        CHECK(state.operation == O::None);
        mouse(MouseButton::Left, CONTROL | OPTION);
        CHECK(state.operation == O::NodeLasso);
        mouse(MouseButton::Left, CONTROL | OPTION, false);
        CHECK(state.operation == O::None);
        state.macCommandAliases = true;
        mouse(MouseButton::Right, COMMAND);
        CHECK(state.operation == O::NodeCut);
        mouse(MouseButton::Right, COMMAND, false);
        key(KeyId::G, COMMAND);
        CHECK(state.operation == O::None && state.confidence != Confidence::Uncertain);
        observe("NONE", "ShaderNodeTree", "SCULPT");
        CHECK(hint(KeyId::G) != NO_HINT &&
              BuildKeyboardStatus(tracked, input).label == "SHADER NODES");
        CHECK(hint(KeyId::V) == NO_HINT); // Backdrop controls apply only to Compositor.
        observe("NONE", "CompositorNodeTree");
        CHECK(hint(KeyId::V) != NO_HINT &&
              BuildKeyboardStatus(tracked, input).label == "COMPOSITOR");
        observe("NONE", "NODE_EDITOR");
        CHECK(hint(KeyId::G) != NO_HINT && BuildKeyboardStatus(tracked, input).label == "NODES");
        observe("NONE", "NODE_EDITOR_UI");
        CHECK(hint(KeyId::G) == NO_HINT);
        observe("NONE", "VIEW_3D", "EDIT_CURVE");
        CHECK(state.editor == BlenderEditor::Viewport && hint(KeyId::E) != NO_HINT);
        TrackInput(tracked, {.type = EventType::NodeEditorBaseline});
        CHECK(state.editor == BlenderEditor::Nodes && !state.observation.connected);
        CHECK(BuildKeyboardStatus(tracked, input).label == "NODES");
        SetAppProfile(tracked, AppProfile::Zed);
        TrackInput(tracked, Key(U'i'));
        TrackInput(tracked, {.type = EventType::NodeEditorBaseline});
        CHECK(tracked.vim.mode == VimMode::Insert);
    }

    void TestCurveEditing()
    {
        using namespace Platform;
        using O = BlenderOperation;
        TrackedState tracked;
        tracked.appProfile = AppProfile::Blender;
        auto& state = tracked.blender;
        std::uint64_t sequence = 0;
        const auto observe =
            [&](std::string_view operation = "NONE", std::string_view mode = "EDIT_CURVE")
        {
            auto report = Observation(mode, operation);
            report.inputSequence = ++sequence;
            ObserveBlenderState(state, report);
        };
        const auto key = [&](KeyId id, std::uint8_t modifiers = 0)
        {
            TrackInput(tracked, {.key = id, .modifiers = modifiers});
        };
        const auto hint = [&](KeyId id, std::uint8_t modifiers = 0)
        {
            return FindBlenderBinding(state, id, modifiers);
        };
        observe();
        CHECK(state.mode == BlenderMode::CurveEdit && state.confidence == Confidence::Observed);
        InputBatchInfo input{
            .foreground = true, .available = true, .appProfile = AppProfile::Blender};
        CHECK(BuildKeyboardStatus(tracked, input).label == "CURVE EDIT");
        CHECK(hint(KeyId::G) != NO_HINT);
        CHECK(BlenderBindings()[hint(KeyId::G)].fullHint.find("control points") !=
              std::string_view::npos);
        CHECK(hint(KeyId::I) == NO_HINT && hint(KeyId::B, CONTROL) == NO_HINT);
        CHECK(hint(KeyId::One) == NO_HINT); // Mesh selection types do not apply to curves.
        for (const auto [id, modifiers] :
             {std::pair{KeyId::G, 0}, {KeyId::D, SHIFT}, {KeyId::A, SHIFT}, {KeyId::F3, 0}})
        {
            std::size_t count = 0;
            for (const auto& binding : BlenderBindings())
            {
                count += binding.key == id && binding.modifiers == modifiers &&
                         BlenderBindingAvailable(binding, state);
            }
            CHECK(count == 1); // Input's first match and the renderer's last match must agree.
        }
        for (const auto [id, modifiers] : {std::pair{KeyId::F, 0},
                                           {KeyId::Y, 0},
                                           {KeyId::P, 0},
                                           {KeyId::C, OPTION},
                                           {KeyId::T, OPTION},
                                           {KeyId::N, SHIFT},
                                           {KeyId::L, 0},
                                           {KeyId::L, SHIFT},
                                           {KeyId::L, CONTROL},
                                           {KeyId::X, CONTROL},
                                           {KeyId::Delete, CONTROL},
                                           {KeyId::NumpadPlus, CONTROL},
                                           {KeyId::NumpadMinus, CONTROL},
                                           {KeyId::Tab, SHIFT},
                                           {KeyId::O, OPTION}})
        {
            CHECK(hint(id, modifiers) != NO_HINT);
            key(id, modifiers);
            CHECK(state.mode == BlenderMode::CurveEdit && state.operation == O::None &&
                  state.context == BlenderContext::Editor &&
                  state.confidence != Confidence::Uncertain);
        }
        for (const auto [id, modifiers] : {std::pair{KeyId::V, 0},
                                           {KeyId::X, 0},
                                           {KeyId::Delete, 0},
                                           {KeyId::A, SHIFT},
                                           {KeyId::G, SHIFT},
                                           {KeyId::O, SHIFT},
                                           {KeyId::H, CONTROL}})
        {
            key(id, modifiers);
            CHECK(state.context == BlenderContext::Menu && state.popupPending);
            observe();
            CHECK(state.context ==
                  BlenderContext::Menu); // An empty modal list cannot close a popup.
            key(KeyId::Enter);
            CHECK(state.mode == BlenderMode::CurveEdit && !state.popupPending &&
                  state.context == BlenderContext::Editor &&
                  state.confidence != Confidence::Uncertain);
        }
        key(KeyId::E);
        observe("CURVE_OT_extrude_move");
        CHECK(state.operation == O::Extrude && state.confidence == Confidence::Observed);
        key(KeyId::X);
        key(KeyId::Two);
        observe("TRANSFORM_OT_translate");
        CHECK(state.operation == O::Extrude && state.axis == 'X' && state.numericInput);
        key(KeyId::Escape);
        observe();
        key(KeyId::D, SHIFT);
        observe("curve_duplicate_move");
        CHECK(state.operation == O::Duplicate);
        key(KeyId::Enter);
        observe("curve_extrude_move");
        CHECK(state.operation == O::Extrude); // A menu/tool-started extrude also has hints.
        observe();
        key(KeyId::T, CONTROL);
        observe("TRANSFORM_OT_tilt");
        CHECK(state.operation == O::Tilt && hint(KeyId::X) == NO_HINT);
        CHECK(hint(KeyId::Shift) != NO_HINT && hint(KeyId::Control) != NO_HINT);
        key(KeyId::Nine);
        key(KeyId::Zero);
        CHECK(BuildKeyboardStatus(tracked, input).label == "TILT" &&
              BuildKeyboardStatus(tracked, input).pending == "NUM");
        key(KeyId::Enter);
        observe();
        key(KeyId::S, OPTION);
        observe("TRANSFORM_OT_transform");
        CHECK(state.operation == O::Radius && state.confidence == Confidence::Inferred);
        CHECK(hint(KeyId::X) == NO_HINT && hint(KeyId::G) == NO_HINT);
        key(KeyId::Tab, SHIFT);
        key(KeyId::Period);
        key(KeyId::Five);
        observe("transform_transform");
        CHECK(state.operation == O::Radius && state.numericInput);
        key(KeyId::Space);
        observe();
        CHECK(state.operation == O::None && state.confidence == Confidence::Observed);
        observe("TRANSFORM_OT_transform");
        CHECK(state.confidence ==
              Confidence::Uncertain); // Do not guess another generic tool as radius.
        observe();
        observe("TRANSFORM_OT_translate"); // Dragging points uses the same move controls as G.
        CHECK(state.operation == O::Move && hint(KeyId::X) != NO_HINT);
        observe();
        CHECK(hint(KeyId::Control) != NO_HINT);
        CHECK(hint(KeyId::Control, CONTROL) != NO_HINT);
        CHECK(hint(KeyId::Control, SHIFT) == NO_HINT);
        TrackInput(tracked, {.type = EventType::MouseAction,
                             .modifiers = CONTROL,
                             .mouseButton = MouseButton::Right});
        CHECK(state.context == BlenderContext::Editor && !state.popupPending);
        state.macCommandAliases = true;
        TrackInput(tracked, {.type = EventType::MouseAction,
                             .modifiers = COMMAND,
                             .mouseButton = MouseButton::Right});
        CHECK(state.context == BlenderContext::Editor && !state.popupPending);
        key(KeyId::T, COMMAND);
        CHECK(state.operation == O::Tilt);
        key(KeyId::Escape);
        key(KeyId::Tab);
        CHECK(state.mode == BlenderMode::Object);
        observe("NONE", "OBJECT");
        key(KeyId::Tab);
        CHECK(state.mode == BlenderMode::CurveEdit);
        observe("NONE", "EDIT_MESH");
        key(KeyId::Tab);
        key(KeyId::Tab);
        CHECK(state.mode == BlenderMode::MeshEdit);
        TrackInput(tracked, {.type = EventType::CurveEditBaseline});
        CHECK(state.mode == BlenderMode::CurveEdit && !state.observation.connected);
        SetAppProfile(tracked, AppProfile::Zed);
        TrackInput(tracked, Key(U'i'));
        TrackInput(tracked, {.type = EventType::CurveEditBaseline});
        CHECK(tracked.vim.mode == VimMode::Insert); // Blender baselines never reset Vim.
    }

    void TestBlenderObservations()
    {
        using namespace Platform;
        using O = BlenderOperation;
        CHECK(ParseBlenderMessage("KC_HELLO 1 123 abc123")->hello);
        for (const auto malformed :
             {"KC_HELLO 2 123 abc", "KC_HELLO 1 -1 abc", "KC_HELLO 1 0 abc",
              "KC_HELLO 1 123 abc extra", "KC_STATE 1 token 123 abc 0 1 OBJECT VIEW_3D 1 1 NONE 0",
              "KC_STATE 1 token 123 abc 1 1 OBJECT VIEW_3D 8 1 NONE 0",
              "KC_STATE 1 token 123 abc 1 1 OBJECT VIEW_3D 1 0 NONE 0",
              "KC_STATE 1 token 123 abc 1 1 OBJECT VIEW_3D 1 2 NONE 0",
              "KC_STATE 1 token 123 abc 1 1 OBJECT VIEW_3D 1 1 NONE 2"})
        {
            CHECK(!ParseBlenderMessage(malformed));
        }
        CHECK(!ParseBlenderMessage(std::string(769, 'x')));
        BlenderState state;
        std::uint64_t sequence = 0;
        const auto observe = [&](BlenderObservation observation)
        {
            observation.inputSequence = ++sequence;
            ObserveBlenderState(state, observation);
        };
        const auto key = [&](KeyId id)
        {
            TrackBlenderInput(state, {.key = id});
        };
        observe(Observation());
        CHECK(state.mode == BlenderMode::MeshEdit && state.selectionMask == 3 &&
              state.confidence == Confidence::Observed);
        const auto revision = state.revision;
        ObserveBlenderState(state, state.observation);
        CHECK(state.revision ==
              revision); // An unchanged heartbeat does not change projected state.
        key(KeyId::G);
        key(KeyId::X);
        CHECK(state.operation == O::Move && state.axis == 'X');
        auto old = Observation("OBJECT");
        old.current = false;
        observe(old);
        CHECK(state.mode == BlenderMode::MeshEdit && state.operation == O::Move &&
              state.axis == 'X');
        observe(Observation("EDIT_MESH", "TRANSFORM_OT_translate"));
        CHECK(state.operation == O::Move && state.axis == 'X');
        observe(Observation());
        CHECK(state.operation == O::None && !state.axis && !state.numericInput);
        InvalidateBlenderState(state, "Undo history is unknown");
        observe(Observation("OBJECT"));
        CHECK(state.mode == BlenderMode::Object && state.confidence == Confidence::Observed);
        observe(Observation("OBJECT", "TRANSFORM_OT_rotate"));
        CHECK(state.operation == O::Rotate);
        observe(Observation("OBJECT", "VIEW3D_OT_walk"));
        CHECK(state.confidence == Confidence::Uncertain &&
              state.uncertainty == BlenderUncertainty::Tool);
        observe(Observation("OBJECT"));
        CHECK(state.confidence == Confidence::Observed && state.operation == O::None);
        key(KeyId::F3);
        observe(Observation("OBJECT"));
        CHECK(state.context == BlenderContext::Search && state.popupPending);
        key(KeyId::Enter);
        CHECK(state.confidence == Confidence::Uncertain && !state.popupPending);
        observe(Observation("OBJECT", "transform_translate"));
        CHECK(state.operation == O::Move && state.confidence == Confidence::Observed);
        observe(Observation("OBJECT"));
        TrackBlenderInput(state,
                          {.type = EventType::MouseAction, .mouseButton = MouseButton::Right});
        TrackBlenderInput(state,
                          {.type = EventType::MouseAction, .mouseButton = MouseButton::Left});
        observe(Observation("OBJECT"));
        CHECK(state.confidence == Confidence::Uncertain && state.popupPending);
        key(KeyId::Escape);
        observe(Observation("OBJECT"));
        CHECK(state.confidence == Confidence::Observed && !state.popupPending);
        observe(Observation("SCULPT"));
        CHECK(state.mode == BlenderMode::Unsupported &&
              FindBlenderBinding(state, KeyId::G, 0) == NO_HINT);
        TrackedState tracked;
        tracked.appProfile = AppProfile::Blender;
        tracked.blender = state;
        InputBatchInfo input{
            .foreground = true, .available = true, .appProfile = AppProfile::Blender};
        CHECK(BuildKeyboardStatus(tracked, input).label == "SCULPT");
        observe(Observation("OBJECT", "NONE", "DOPESHEET_EDITOR"));
        CHECK(FindBlenderBinding(state, KeyId::G, 0) == NO_HINT);
        tracked.blender = state;
        CHECK(BuildKeyboardStatus(tracked, input).label == "DOPESHEET_EDITOR");
        observe(Observation("OBJECT"));
        CHECK(FindBlenderBinding(state, KeyId::G, 0) != NO_HINT);
        auto locked = Observation("OBJECT");
        locked.interfaceLocked = true;
        observe(locked);
        CHECK(state.confidence == Confidence::Uncertain);
        observe(Observation("OBJECT"));
        CHECK(state.confidence == Confidence::Observed);
        observe({});
        CHECK(!state.observation.connected && state.confidence == Confidence::Uncertain);
        observe(Observation());
        CHECK(state.confidence == Confidence::Observed);
        // 4.1 confirms the base mode without pretending to know whether a tool has ended.
        key(KeyId::G);
        observe(Observation("EDIT_MESH", "UNAVAILABLE"));
        CHECK(state.operation == O::Move);
        InvalidateBlenderState(state, "Unknown operation");
        observe(Observation("EDIT_MESH", "UNAVAILABLE"));
        CHECK(state.uncertainty == BlenderUncertainty::Tool &&
              state.confidence == Confidence::Uncertain);
        key(KeyId::Escape);
        observe(Observation("EDIT_MESH", "UNAVAILABLE"));
        CHECK(state.mode == BlenderMode::MeshEdit && state.confidence == Confidence::Observed);
    }
} // namespace

int main()
{
    TestBlenderObservations();
    TestCurveEditing();
    TestNodeEditing();
    using namespace Platform;
    using O = BlenderOperation;
    using C = BlenderContext;
    TrackedState tracked;
    SetAppProfile(tracked, AppProfile::Blender);
    auto& state = tracked.blender;
    const auto key = [&](KeyId id, std::uint8_t mods = 0, bool repeat = false)
    {
        // Blender uses the physical chord, even if Option/Caps changes the Unicode value.
        TrackInput(tracked, {.key = id, .value = {U'ø'}, .modifiers = mods, .repeat = repeat});
    };
    const auto mouse = [&](MouseButton button, bool pressed = true)
    {
        TrackInput(
            tracked,
            {.type = EventType::MouseAction, .mouseButton = button, .mousePressed = pressed});
    };
    const auto baseline = [&](BlenderMode mode = BlenderMode::MeshEdit)
    {
        TrackInput(tracked, {.type = mode == BlenderMode::Object ? EventType::ObjectBaseline
                                                                 : EventType::MeshEditBaseline});
    };
    CHECK(state.mode == BlenderMode::Object && state.confidence == Confidence::Assumed);
    key(KeyId::Tab);
    CHECK(state.mode == BlenderMode::MeshEdit);
    key(KeyId::Tab, 0, true);
    CHECK(state.mode == BlenderMode::MeshEdit);
    key(KeyId::Two);
    CHECK(state.selectionMask == 2);
    key(KeyId::One, SHIFT);
    CHECK(state.selectionMask == 3);
    key(KeyId::G);
    CHECK(state.operation == O::Move);
    key(KeyId::X);
    CHECK(state.axis == 'X' && state.constraintCycle == 1);
    key(KeyId::X);
    CHECK(state.axis == 'X' && state.constraintCycle == 2);
    key(KeyId::X);
    CHECK(!state.axis && state.constraint == BlenderConstraint::None);
    key(KeyId::Z, SHIFT);
    CHECK(state.axis == 'Z' && state.constraint == BlenderConstraint::Plane);
    key(KeyId::Three);
    CHECK(state.numericInput && state.selectionMask == 3);
    TrackInput(tracked, {.type = EventType::Modifiers, .modifiers = SHIFT | CONTROL});
    CHECK(state.precision && state.snapInverted);
    TrackInput(tracked, {.type = EventType::Modifiers, .modifiers = COMMAND});
    CHECK(!state.precision && !state.snapInverted);
    InputBatchInfo input{.foreground = true, .available = true, .appProfile = AppProfile::Blender};
    const auto status = BuildKeyboardStatus(tracked, input);
    CHECK(status.label == "MOVE" && status.pending == "PLANE Z · NUM");
    CHECK(status.pending.find('3') == std::string::npos);
    key(KeyId::Enter, SHIFT | CONTROL);
    CHECK(state.operation == O::None && state.mode == BlenderMode::MeshEdit);
    key(KeyId::R);
    key(KeyId::R);
    CHECK(state.operation == O::Trackball);
    key(KeyId::Escape);
    CHECK(state.operation == O::None && state.mode == BlenderMode::MeshEdit);
    key(KeyId::G);
    key(KeyId::G);
    CHECK(state.operation == O::EdgeSlide);
    mouse(MouseButton::Right);
    CHECK(state.operation == O::None && state.mode == BlenderMode::MeshEdit);
    key(KeyId::E);
    CHECK(state.operation == O::Extrude);
    mouse(MouseButton::Left);
    CHECK(state.operation == O::None);
    key(KeyId::D, SHIFT);
    CHECK(state.operation == O::Duplicate);
    key(KeyId::Escape);
    CHECK(state.operation == O::None);
    key(KeyId::I);
    CHECK(state.operation == O::Inset);
    key(KeyId::B);
    key(KeyId::Enter);
    CHECK(state.operation == O::None);
    key(KeyId::B, CONTROL);
    CHECK(state.operation == O::Bevel);
    key(KeyId::P);
    key(KeyId::Four);
    CHECK(state.operation == O::Bevel && state.numericInput);
    mouse(MouseButton::Right);
    CHECK(state.operation == O::None);
    key(KeyId::K);
    CHECK(state.operation == O::Knife);
    mouse(MouseButton::Left);
    mouse(MouseButton::Right);
    CHECK(state.operation == O::Knife);
    key(KeyId::C);
    key(KeyId::Enter);
    CHECK(state.operation == O::None);
    key(KeyId::R, CONTROL);
    CHECK(state.operation == O::LoopCut);
    key(KeyId::Two);
    mouse(MouseButton::Left);
    CHECK(state.operation == O::EdgeSlide && !state.numericInput);
    mouse(MouseButton::Right);
    CHECK(state.operation == O::None);
    key(KeyId::B);
    mouse(MouseButton::Left);
    CHECK(state.operation == O::BoxSelect);
    mouse(MouseButton::Left, false);
    CHECK(state.operation == O::None);
    key(KeyId::C);
    mouse(MouseButton::Left);
    mouse(MouseButton::Left, false);
    CHECK(state.operation == O::CircleSelect);
    mouse(MouseButton::Right);
    CHECK(state.operation == O::None);
    mouse(MouseButton::Left);
    mouse(MouseButton::Middle);
    CHECK(state.confidence == Confidence::Assumed);
    mouse(MouseButton::Right);
    CHECK(state.context == C::Menu);
    key(KeyId::Escape);
    CHECK(state.context == C::Editor);
    key(KeyId::M);
    CHECK(state.context == C::Menu);
    key(KeyId::Enter);
    CHECK(state.context == C::Editor && state.mode == BlenderMode::MeshEdit &&
          state.confidence == Confidence::Assumed);
    key(KeyId::Tab, CONTROL);
    CHECK(state.context == C::Menu && state.menu == BlenderMenuKind::Unknown);
    key(KeyId::Enter);
    CHECK(state.confidence == Confidence::Uncertain);
    key(KeyId::Escape);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain);
    CHECK(BuildKeyboardStatus(tracked, input).label == "RESYNC");
    baseline();
    key(KeyId::F3);
    CHECK(state.context == C::Search);
    for (char c : std::string_view("sculpt-secret-scene-name"))
    {
        key(static_cast<KeyId>(c >= 'a' && c <= 'z' ? c - 32 : c));
    }
    CHECK(state.context == C::Search && BuildKeyboardStatus(tracked, input).pending.empty());
    key(KeyId::Escape);
    CHECK(state.confidence != Confidence::Uncertain && state.mode == BlenderMode::MeshEdit);
    key(KeyId::F3);
    key(KeyId::Enter);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    key(KeyId::F2);
    key(KeyId::A);
    key(KeyId::Enter);
    CHECK(state.context == C::Editor);
    baseline(BlenderMode::Object);
    key(KeyId::X);
    CHECK(state.context == C::DeleteConfirm);
    key(KeyId::Enter);
    CHECK(state.context == C::Editor && state.mode == BlenderMode::Object);
    CHECK(FindBlenderBinding(state, KeyId::One, 0) == NO_HINT);
    CHECK(FindBlenderBinding(state, KeyId::NumpadOne, 0) != NO_HINT);
    CHECK(FindBlenderBinding(state, KeyId::E, 0) == NO_HINT);
    CHECK(FindBlenderBinding(state, KeyId::A, COMMAND) == NO_HINT);
    state.macCommandAliases = true;
    CHECK(FindBlenderBinding(state, KeyId::A, COMMAND) != NO_HINT);
    CHECK(FindBlenderBinding(state, KeyId::Tab, COMMAND) == NO_HINT);
    CHECK(FindBlenderBinding(state, KeyId::H, COMMAND) == NO_HINT);
    baseline();
    key(KeyId::B, COMMAND);
    CHECK(state.operation == O::Bevel);
    key(KeyId::Escape);
    key(KeyId::G);
    CHECK(FindBlenderBinding(state, KeyId::Control, COMMAND) != NO_HINT);
    TrackInput(tracked, {.type = EventType::Modifiers, .modifiers = COMMAND});
    CHECK(!state.snapInverted);
    TrackFocusChange(tracked, false);
    CHECK(state.confidence == Confidence::Uncertain &&
          state.uncertainty == BlenderUncertainty::Tool);
    TrackFocusChange(tracked, true);
    CHECK(state.confidence == Confidence::Uncertain);
    CHECK(BuildKeyboardStatus(tracked, input).detail.find("Press Escape") != std::string::npos);
    key(KeyId::Escape, 0, true);
    CHECK(state.confidence == Confidence::Uncertain);
    key(KeyId::Escape);
    CHECK(state.mode == BlenderMode::MeshEdit && state.confidence == Confidence::Assumed &&
          state.uncertainty == BlenderUncertainty::None);
    baseline();
    TrackFocusChange(tracked, false);
    TrackFocusChange(tracked, true);
    CHECK(state.mode == BlenderMode::MeshEdit && state.confidence == Confidence::Assumed);
    SetAppProfile(tracked, AppProfile::Zed);
    TrackInput(tracked, Key(U'i'));
    CHECK(tracked.vim.mode == VimMode::Insert);
    SetAppProfile(tracked, AppProfile::Blender);
    CHECK(state.mode == BlenderMode::MeshEdit);
    TrackInput(tracked, {.type = EventType::NormalBaseline});
    CHECK(state.mode == BlenderMode::MeshEdit);
    key(KeyId::F12);
    CHECK(state.confidence == Confidence::Uncertain);
    for (auto mode : {BlenderMode::Object, BlenderMode::MeshEdit})
    {
        baseline(mode);
        key(KeyId::F1);
        key(KeyId::F11, CONTROL);
        key(KeyId::F9); // Without a redoable operation, Blender opens nothing.
        key(KeyId::G);
        CHECK(state.mode == mode && state.confidence != Confidence::Uncertain &&
              state.context == C::Editor && state.operation == O::Move);
        key(KeyId::Escape);
        key(KeyId::F4);
        CHECK(state.context == C::Menu && state.popupPending);
        key(KeyId::Escape);
        CHECK(state.mode == mode && state.context == C::Editor && !state.popupPending);
        // macOS keeps Control-F2 and F11 by default; Command-F2 still reaches Blender.
        CHECK(FindBlenderBinding(state, KeyId::F2, CONTROL) == NO_HINT &&
              FindBlenderBinding(state, KeyId::F11, 0) == NO_HINT);
        key(KeyId::F2, CONTROL);
        key(KeyId::F11);
        CHECK(state.context == C::Editor && state.confidence != Confidence::Uncertain);
        key(KeyId::F2, COMMAND);
        CHECK(state.context == C::Rename);
        key(KeyId::Escape);
        CHECK(state.mode == mode && state.context == C::Editor);
        key(KeyId::F5, SHIFT);
        CHECK(state.mode == mode && state.editor == BlenderEditor::Viewport &&
              state.confidence != Confidence::Uncertain);
        TrackInput(tracked, {.type = EventType::NodeEditorBaseline});
        key(KeyId::F5, SHIFT);
        CHECK(state.mode == mode && state.editor == BlenderEditor::Viewport &&
              state.confidence != Confidence::Uncertain);
        key(KeyId::F3, SHIFT);
        CHECK(state.confidence == Confidence::Uncertain); // Resulting editor needs observation.
    }
    // System shortcuts do not escalate an interrupted tool beyond Escape recovery.
    baseline();
    key(KeyId::G);
    TrackFocusChange(tracked, false);
    key(KeyId::F11);
    key(KeyId::F3, CONTROL);
    CHECK(state.uncertainty == BlenderUncertainty::Tool);
    key(KeyId::Escape);
    CHECK(state.confidence != Confidence::Uncertain && state.mode == BlenderMode::MeshEdit);
    state.macCommandAliases = false;
    CHECK(FindBlenderBinding(state, KeyId::F11, 0) != NO_HINT);
    key(KeyId::F2, CONTROL);
    CHECK(state.context == C::Rename);
    key(KeyId::Escape);
    state.macCommandAliases = true;
    baseline();
    key(KeyId::Z, CONTROL);
    CHECK(state.confidence == Confidence::Uncertain);
    CHECK(state.uncertainty == BlenderUncertainty::Mode);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain);
    // Common default controls must leave Object/Edit usable rather than forcing a baseline.
    for (const auto mode : {BlenderMode::Object, BlenderMode::MeshEdit})
    {
        baseline(mode);
        key(KeyId::S, CONTROL);
        key(KeyId::Z, OPTION);
        key(KeyId::Z, SHIFT);
        key(KeyId::Grave, CONTROL);
        key(KeyId::Space);
        key(KeyId::Space, CONTROL | SHIFT);
        key(KeyId::Space, CONTROL);
        key(KeyId::Left);
        key(KeyId::Right);
        key(KeyId::Left, SHIFT);
        key(KeyId::Right, SHIFT);
        key(KeyId::Up);
        key(KeyId::Down);
        key(KeyId::F5);
        key(KeyId::F10);
        CHECK(state.mode == mode && state.operation == O::None && state.context == C::Editor &&
              state.confidence != Confidence::Uncertain);
    }
    baseline(BlenderMode::Object);
    key(KeyId::Three, CONTROL);
    key(KeyId::A, COMMAND);
    CHECK(state.context == C::Menu && state.menu == BlenderMenuKind::PreserveMode);
    mouse(MouseButton::Left);
    CHECK(state.mode == BlenderMode::Object && state.confidence == Confidence::Assumed);
    baseline();
    key(KeyId::Two, CONTROL);
    key(KeyId::One, CONTROL | SHIFT);
    CHECK(state.selectionMask == 3 && state.confidence != Confidence::Uncertain);
    key(KeyId::Z);
    key(KeyId::Two);
    CHECK(state.context == C::Editor && state.mode == BlenderMode::MeshEdit);
    key(KeyId::X);
    mouse(MouseButton::Left);
    CHECK(state.context == C::Editor && state.confidence == Confidence::Assumed);
    key(KeyId::M);
    key(KeyId::Down);
    key(KeyId::Right);
    key(KeyId::Left);
    key(KeyId::NumpadEnter);
    CHECK(state.context == C::Editor && state.confidence != Confidence::Uncertain);
    key(KeyId::Q);
    mouse(MouseButton::Right);
    CHECK(state.context == C::Editor && state.confidence != Confidence::Uncertain);
    key(KeyId::Q);
    key(KeyId::A);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain &&
          state.uncertainty == BlenderUncertainty::Mode);
    baseline();
    key(KeyId::F3);
    mouse(MouseButton::Right);
    CHECK(state.context == C::Editor && state.confidence != Confidence::Uncertain);
    key(KeyId::F2);
    mouse(MouseButton::Left);
    CHECK(state.context == C::Rename);
    mouse(MouseButton::Right);
    CHECK(state.context == C::Editor && state.confidence != Confidence::Uncertain);
    key(KeyId::G);
    key(KeyId::One);
    key(KeyId::Slash);
    key(KeyId::Two);
    key(KeyId::Equal, SHIFT);
    key(KeyId::Eight, SHIFT);
    key(KeyId::NumpadMultiply);
    key(KeyId::NumpadPlus);
    key(KeyId::PageUp, SHIFT);
    key(KeyId::B);
    CHECK(state.operation == O::Move && state.numericInput &&
          state.confidence != Confidence::Uncertain);
    CHECK(BuildKeyboardStatus(tracked, input).pending == "NUM");
    key(KeyId::Enter);
    key(KeyId::B, CONTROL);
    key(KeyId::NumpadPlus);
    key(KeyId::NumpadMinus);
    CHECK(state.operation == O::Bevel && !state.numericInput);
    key(KeyId::Escape);
    key(KeyId::R, CONTROL);
    key(KeyId::NumpadTwo);
    CHECK(state.operation == O::LoopCut && state.numericInput);
    mouse(MouseButton::Left);
    mouse(MouseButton::Left);
    CHECK(state.operation == O::None && state.mode == BlenderMode::MeshEdit);
    key(KeyId::G);
    key(KeyId::W);
    CHECK(state.confidence == Confidence::Uncertain &&
          state.uncertainty == BlenderUncertainty::Tool);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Assumed && state.mode == BlenderMode::MeshEdit);
    key(KeyId::G);
    TrackFocusChange(tracked, false);
    key(KeyId::Tab);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain &&
          state.uncertainty == BlenderUncertainty::Mode);
    baseline();
    key(KeyId::G);
    TrackFocusChange(tracked, false);
    TrackInput(tracked, {.type = EventType::KeyUp, .key = KeyId::G});
    TrackInput(tracked, {.type = EventType::Modifiers, .modifiers = COMMAND});
    key(KeyId::Tab, COMMAND);
    CHECK(state.uncertainty == BlenderUncertainty::Tool);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Assumed);
    key(KeyId::G);
    TrackFocusChange(tracked, false);
    InvalidateTrackedState(tracked, "Observation lost");
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain &&
          state.uncertainty == BlenderUncertainty::Mode);
    InvalidateBlenderState(state, "Later tool interruption", BlenderUncertainty::Tool);
    key(KeyId::Escape);
    CHECK(state.uncertainty == BlenderUncertainty::Mode &&
          state.confidence == Confidence::Uncertain);
    CHECK(BuildKeyboardStatus(tracked, input).detail.find("KeyGuide → Resync Mode") !=
          std::string::npos);
    baseline();
    key(KeyId::G);
    key(KeyId::O, CONTROL);
    CHECK(state.uncertainty == BlenderUncertainty::Mode);
    key(KeyId::Escape);
    CHECK(state.confidence == Confidence::Uncertain);
    baseline();
    TrackInput(tracked, {.type = EventType::Unsupported});
    CHECK(state.confidence == Confidence::Uncertain);
    CHECK(FindBlenderBinding(state, KeyId::Escape, 0) == NO_HINT);
    input.foreground = false;
    CHECK(BuildKeyboardStatus(tracked, input).label == "INACTIVE");
    input.foreground = true;
    input.paused = true;
    CHECK(BuildKeyboardStatus(tracked, input).label == "PAUSED");
    input.paused = false;
    input.available = false;
    CHECK(BuildKeyboardStatus(tracked, input).label == "INPUT OFF");
    std::cout << "Passive Blender mode / modal / mouse / context / resync checks passed\n";
}
