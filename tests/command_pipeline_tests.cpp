// tests/command_pipeline_tests.cpp
#include "test_helpers.hpp"
#include "platform_stub.hpp"
#include <cmath>
#include "gameLogic/input.hpp"
#include "gameLogic/keyboard_status.hpp"
#include "gameLogic/vim_bindings.hpp"
#include "gameLogic/tick_logic.hpp"
#include "global/global.hpp"
#include "registry/global.hpp"
#include "render/keyboard_geometry.hpp"
#include "render/shapes.hpp"
#include "render/text_layout.hpp"
#include "settings/defaults.hpp"
#include "settings/settings.hpp"
#include "platform/blender_protocol.hpp"

namespace
{
    void TestFunctionLayer()
    {
        using namespace Platform;
        const auto display = [&](KeyId key)
        {
            for (auto entity : registry.getTag(TagName::Key))
            {
                if (registry.getComponent<Key_Values>(entity)->key == key)
                {
                    return registry.getComponent<Key_Display>(entity);
                }
            }
            std::exit(1);
        };
        const auto pump = [&]
        {
            UpdateInputInfo();
            cmd_registry.executeAll();
            CommandProjectKeyboard{}.execute();
        };
        const auto emit = [&](InputEvent event)
        {
            event.foregroundGeneration = TestPlatform::status.foregroundGeneration;
            event.lossGeneration = TestPlatform::status.lossGeneration;
            TestPlatform::events.push_back(event);
            pump();
        };
        const auto hint = [&](KeyId key)
        {
            const auto id = display(key)->hint;
            return id == NO_HINT ? std::string_view{} : GetShortcutHint(id).shortHint;
        };
        const auto count = registry.getTag(TagName::Key).size();
        const auto canvas = KeyboardCanvasSize();
        const std::array<KeyId, 12> numbers{KeyId::One,  KeyId::Two,  KeyId::Three, KeyId::Four,
                                            KeyId::Five, KeyId::Six,  KeyId::Seven, KeyId::Eight,
                                            KeyId::Nine, KeyId::Zero, KeyId::Minus, KeyId::Equal};
        for (std::size_t i = 0; i < numbers.size(); ++i)
        {
            CHECK(FunctionKeyForNumber(numbers[i]) ==
                  static_cast<KeyId>(static_cast<int>(KeyId::F1) + i));
        }
        CHECK(FunctionKeyForNumber(KeyId::Grave) == KeyId::Unknown);
        CHECK(FunctionKeyForNumber(KeyId::NumpadOne) == KeyId::Unknown);
        TestPlatform::pointer.inside = false;
        TestPlatform::status = {.foregroundGeneration = 400,
                                .foreground = true,
                                .available = true,
                                .appProfile = AppProfile::Blender};
        TestPlatform::events.clear();
        ResetInputInfo();
        pump();
        emit({.type = EventType::MeshEditBaseline});
        CHECK(hint(KeyId::Two) == "edges");
        CHECK(display(KeyId::Two)->functionAvailable && !display(KeyId::Two)->functionLayer);
        const auto number_hint = display(KeyId::Two)->hint;
        auto& tracked = registry.getComponent<Tracked_State>(Global::trackerEID)->value;
        emit({.type = EventType::Modifiers, .functionHeld = true});
        CHECK(Global::Input::functionHeld && hint(KeyId::Two) == "rename");
        CHECK(hint(KeyId::Three) == "search" && hint(KeyId::Nine) == "adjust");
        CHECK(hint(KeyId::Minus) == "render" && hint(KeyId::Equal) == "render");
        CHECK(hint(KeyId::Five).empty() && display(KeyId::Five)->functionAvailable);
        CHECK(display(KeyId::Five)->availableModifiers == SHIFT);
        CHECK(tracked.blender.mode == BlenderMode::MeshEdit &&
              tracked.blender.context == BlenderContext::Editor);
        // macOS keeps F11 for Show Desktop by default; Shift/Control-F11 still reach Blender.
        TestPlatform::status.macCommandAliases = true;
        pump();
        CHECK(hint(KeyId::Minus).empty() && display(KeyId::Minus)->functionAvailable);
        TestPlatform::status.macCommandAliases = false;
        pump();
        const Key_Values two{KeyId::Two, {KeyValue{U'2'}, KeyValue{U'@'}}, KeyId::F2};
        CHECK(HintChord(two, GetShortcutHint(display(KeyId::Two)->hint), true) == "Fn-2 / F2");
        CHECK(KeyName(DisplayedKeyValues(two, display(KeyId::Two)->functionLayer), 0) == "F2");
        emit({.type = EventType::Modifiers, .modifiers = SHIFT, .functionHeld = true});
        for (auto number : numbers)
        {
            CHECK(display(number)->functionAvailable && display(number)->hint != NO_HINT);
            CHECK(GetShortcutHint(display(number)->hint).modifiers == SHIFT);
        }
        CHECK(hint(KeyId::Five) == "3D view");
        CHECK(HintChord(two, GetShortcutHint(display(KeyId::Two)->hint), true) ==
              "Fn-Shift-2 / Shift-F2");
        emit({.type = EventType::Modifiers, .modifiers = CONTROL | SHIFT, .functionHeld = true});
        CHECK(!display(KeyId::Two)->functionAvailable && hint(KeyId::Two).empty());
        emit({.type = EventType::Modifiers});
        CHECK(!Global::Input::functionHeld && display(KeyId::Two)->hint == number_hint);
        // Fn remains inspectable while mode/hint context is frozen over the overlay.
        TestPlatform::pointer.inside = true;
        emit({.type = EventType::Modifiers, .functionHeld = true});
        emit({.type = EventType::NodeEditorBaseline});
        CHECK(tracked.blender.editor == BlenderEditor::Nodes && hint(KeyId::Two) == "rename");
        CHECK(GetShortcutHint(display(KeyId::Two)->hint).fullHint == "rename active object");
        emit({.type = EventType::Modifiers});
        CHECK(display(KeyId::Two)->hint == number_hint);
        TestPlatform::pointer.inside = false;
        pump();
        CHECK(hint(KeyId::Two).empty());
        emit({.type = EventType::MeshEditBaseline});
        // A delivered function key reveals the same layer even when hardware hides Fn itself.
        emit({.key = KeyId::F2});
        CHECK(display(KeyId::Two)->pressed && display(KeyId::Two)->functionLayer);
        CHECK(tracked.blender.context == BlenderContext::Rename);
        CHECK(hint(KeyId::Two).empty()); // The rename popup has its own scoped hints.
        emit({.type = EventType::KeyUp, .key = KeyId::F2});
        CHECK(!display(KeyId::Two)->pressed && !display(KeyId::Two)->functionLayer);
        emit({.key = KeyId::Escape});
        CHECK(display(KeyId::Two)->hint == number_hint);
        // A lost F-key release keeps its physical feedback, but the next key press ends the layer.
        emit({.key = KeyId::F1});
        CHECK(display(KeyId::One)->pressed && display(KeyId::Two)->functionLayer);
        emit({.key = KeyId::Escape});
        CHECK(display(KeyId::One)->pressed && !display(KeyId::Two)->functionLayer);
        CHECK(display(KeyId::Two)->hint == number_hint);
        emit({.type = EventType::KeyUp, .key = KeyId::F1});
        // Fn typed a number, so this keyboard's Fn does not produce F keys. The tracker still
        // receives the number; only delivered F keys reveal the layer from now on.
        emit({.type = EventType::Modifiers, .functionHeld = true});
        CHECK(display(KeyId::Two)->functionLayer);
        emit({.key = KeyId::Two, .value = {U'2'}, .functionHeld = true});
        CHECK(tracked.blender.context == BlenderContext::Editor &&
              tracked.blender.selectionMask == 2 && Global::Input::functionTypesNumbers);
        CHECK(!display(KeyId::Two)->functionLayer && display(KeyId::Two)->hint == number_hint);
        emit({.type = EventType::KeyUp, .key = KeyId::Two, .functionHeld = true});
        emit({.type = EventType::Modifiers});
        emit({.type = EventType::Modifiers, .functionHeld = true});
        CHECK(!display(KeyId::Two)->functionLayer && display(KeyId::Two)->hint == number_hint);
        emit({.key = KeyId::F2, .functionHeld = true});
        CHECK(display(KeyId::Two)->functionLayer);
        emit({.type = EventType::KeyUp, .key = KeyId::F2, .functionHeld = true});
        emit({.key = KeyId::Escape, .functionHeld = true});
        cmd_registry.add<CommandInvalidateState>("Function layer fixture");
        cmd_registry.executeAll();
        pump();
        for (auto number : numbers)
        {
            CHECK(!display(number)->functionAvailable && display(number)->hint == NO_HINT &&
                  !display(number)->availableModifiers);
        }
        TestPlatform::status.foreground = false;
        ++TestPlatform::status.foregroundGeneration;
        emit({.key = KeyId::F3, .functionHeld = true});
        CHECK(display(KeyId::Three)->pressed && !display(KeyId::Three)->functionAvailable &&
              display(KeyId::Three)->hint == NO_HINT);
        ++TestPlatform::status.lossGeneration;
        pump();
        CHECK(!Global::Input::functionHeld && !display(KeyId::Three)->pressed);
        CHECK(Global::Input::functionKey == KeyId::Unknown && Global::Input::functionTypesNumbers);
        // Every profile shares the layer; F keys have no Vim hints, so it shows none.
        TestPlatform::status.appProfile = AppProfile::Zed;
        TestPlatform::status.foreground = true;
        ++TestPlatform::status.foregroundGeneration;
        ResetInputInfo();
        pump();
        const auto vim_hint = display(KeyId::Two)->hint;
        CHECK(vim_hint != NO_HINT && !display(KeyId::Two)->functionAvailable);
        emit({.type = EventType::Modifiers, .functionHeld = true});
        CHECK(display(KeyId::Two)->functionLayer && display(KeyId::Two)->hint == NO_HINT &&
              !display(KeyId::Two)->functionAvailable);
        emit({.type = EventType::Modifiers});
        CHECK(display(KeyId::Two)->hint == vim_hint && !display(KeyId::Two)->functionLayer);
        emit({.key = KeyId::F2});
        CHECK(display(KeyId::Two)->pressed && display(KeyId::Two)->functionLayer);
        emit({.type = EventType::KeyUp, .key = KeyId::F2});
        CHECK(registry.getTag(TagName::Key).size() == count);
        CHECK(KeyboardCanvasSize().x == canvas.x && KeyboardCanvasSize().y == canvas.y);
    }

    void TestAeroSpace()
    {
        using namespace Platform;
        const auto saved_status = TestPlatform::status;
        const auto saved_pointer = TestPlatform::pointer;
        const auto display = [&](KeyId key)
        {
            for (auto entity : registry.getTag(TagName::Key))
            {
                if (registry.getComponent<Key_Values>(entity)->key == key)
                {
                    return registry.getComponent<Key_Display>(entity);
                }
            }
            std::exit(1);
        };
        const auto pump = [&]
        {
            UpdateInputInfo();
            cmd_registry.executeAll();
            CommandProjectKeyboard{}.execute();
        };
        const auto emit = [&](InputEvent event)
        {
            event.foregroundGeneration = TestPlatform::status.foregroundGeneration;
            event.lossGeneration = TestPlatform::status.lossGeneration;
            TestPlatform::events.push_back(event);
            pump();
        };
        const auto hint = [&](KeyId key)
        {
            const auto id = display(key)->hint;
            return id == NO_HINT ? std::string_view{} : GetShortcutHint(id).shortHint;
        };
        const auto& tracked = registry.getComponent<Tracked_State>(Global::trackerEID)->value;
        const auto status = [&]
        {
            return BuildKeyboardStatus(tracked, Global::Input::status,
                                       Global::Input::aerospaceService);
        };
        TestPlatform::pointer.inside = false;
        TestPlatform::status = {
            .foregroundGeneration = 500, .available = true, .appProfile = AppProfile::Zed};
        TestPlatform::events.clear();
        ResetInputInfo();
        pump();
        CHECK(hint(KeyId::H).empty() && display(KeyId::H)->availableModifiers == (OPTION | SHIFT));
        CHECK(status().label == "INACTIVE" && status().pending == "AEROSPACE");
        emit({.type = EventType::Modifiers, .modifiers = OPTION});
        CHECK(hint(KeyId::H) == "left" && hint(KeyId::Two) == "ws 2" && hint(KeyId::A) == "ws A");
        CHECK(hint(KeyId::Comma) == "accordion" && hint(KeyId::Semicolon).empty());
        CHECK(GetShortcutHint(display(KeyId::H)->hint).physical);
        emit({.type = EventType::Modifiers, .modifiers = OPTION | SHIFT});
        CHECK(hint(KeyId::H) == "move" && hint(KeyId::Two) == "send 2");
        CHECK(hint(KeyId::Semicolon) == "service");
        emit({.key = KeyId::Semicolon, .modifiers = OPTION | SHIFT});
        CHECK(Global::Input::aerospaceService && hint(KeyId::H) == "join" &&
              hint(KeyId::Two).empty());
        emit({.type = EventType::KeyUp, .key = KeyId::Semicolon, .modifiers = OPTION | SHIFT});
        emit({.type = EventType::Modifiers});
        CHECK(hint(KeyId::R) == "reset" && hint(KeyId::F) == "float" &&
              hint(KeyId::Backspace) == "close others");
        CHECK(status().pending == "AEROSPACE · SERVICE");
        // Unbound keys pass through AeroSpace and keep service mode.
        emit({.key = KeyId::J});
        CHECK(Global::Input::aerospaceService);
        emit({.key = KeyId::R});
        CHECK(!Global::Input::aerospaceService && hint(KeyId::R).empty());
        TestPlatform::status.paused = true;
        emit({.type = EventType::Modifiers, .modifiers = OPTION});
        CHECK(hint(KeyId::H).empty() && status().pending.empty());
        TestPlatform::status.paused = false;
        TestPlatform::status.foreground = true;
        ++TestPlatform::status.foregroundGeneration;
        emit({.type = EventType::Modifiers, .modifiers = OPTION});
        CHECK(!AeroSpaceShortcutAvailable(display(KeyId::H)->hint, false) &&
              status().pending.empty());
        TestPlatform::status = saved_status;
        TestPlatform::pointer = saved_pointer;
        TestPlatform::events.clear();
        ResetInputInfo();
        pump();
    }

    void TestHoverPresentation()
    {
        using namespace Platform;
        auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
        CHECK(tracked);
        const auto key_display = [&](KeyId key)
        {
            for (auto entity : registry.getTag(TagName::Key))
            {
                if (registry.getComponent<Key_Values>(entity)->key == key)
                {
                    return registry.getComponent<Key_Display>(entity);
                }
            }
            std::exit(1);
        };
        const auto pump = [&]
        {
            const bool dirty = UpdateInputInfo();
            cmd_registry.executeAll();
            if (dirty)
            {
                cmd_registry.add<CommandProjectKeyboard>();
                cmd_registry.executeAll();
            }
            return dirty;
        };
        const auto emit = [&](InputEvent event)
        {
            event.foregroundGeneration = TestPlatform::status.foregroundGeneration;
            event.lossGeneration = TestPlatform::status.lossGeneration;
            TestPlatform::events.push_back(event);
        };
        const auto status = [&]
        {
            return BuildKeyboardStatus(tracked->displayedState(),
                                       tracked->displayedInput(Global::Input::status));
        };
        std::string description = "Original display explanation";
        TestPlatform::status = {.foregroundGeneration = 100,
                                .foreground = true,
                                .available = true,
                                .description = description.c_str()};
        TestPlatform::pointer = {0, 0, false};
        TestPlatform::events.clear();
        ResetInputInfo();
        CHECK(pump());
        CHECK(status().label == "NORMAL");
        const auto normal_hint = key_display(KeyId::U)->hint;
        CHECK(normal_hint != NO_HINT);
        const auto normal_description = std::string(GetShortcutHint(normal_hint).fullHint);
        // Enter over the window margin in the same batch as a mode-changing key.
        TestPlatform::pointer.inside = true;
        emit(Key(U'i'));
        CHECK(pump());
        CHECK(tracked->value.vim.mode == VimMode::Insert);
        CHECK(tracked->presentation.hoverLocked && status().label == "NORMAL");
        CHECK(key_display(KeyId::U)->hint == normal_hint);
        CHECK(GetShortcutHint(key_display(KeyId::U)->hint).fullHint == normal_description);
        CHECK(key_display(KeyId::I)->pressed);
        description = "New live explanation with different storage";
        TestPlatform::status.description = description.c_str();
        emit({.type = EventType::KeyUp, .key = KeyId::I});
        emit({.type = EventType::Modifiers, .modifiers = CONTROL});
        CHECK(pump());
        CHECK(!key_display(KeyId::I)->pressed && Global::Input::modifiers == CONTROL);
        CHECK(status().label == "NORMAL" &&
              status().detail.find("Original display explanation") == 0);
        CHECK(key_display(KeyId::U)->hint != normal_hint && key_display(KeyId::U)->hint != NO_HINT);
        CHECK(GetShortcutHint(key_display(KeyId::U)->hint).modifiers == CONTROL);
        emit({.type = EventType::Modifiers});
        CHECK(pump());
        ++TestPlatform::status.lossGeneration;
        CHECK(pump());
        CHECK(tracked->value.vim.confidence == Confidence::Uncertain);
        CHECK(status().kind == StatusKind::Mode && status().label == "NORMAL");
        CHECK(key_display(KeyId::U)->hint == normal_hint);
        TestPlatform::status.available = false;
        CHECK(pump());
        CHECK(status().label == "NORMAL");
        TestPlatform::status.available = true;
        TestPlatform::status.appProfile = AppProfile::Blender;
        ++TestPlatform::status.foregroundGeneration;
        auto observed =
            ParseBlenderMessage("KC_STATE 1 token 123 addon 1 1 OBJECT GeometryNodeTree 1 1 NONE 0")
                ->observation;
        observed.current = true;
        TestPlatform::status.blenderObservation = observed;
        CHECK(pump());
        CHECK(tracked->value.appProfile == AppProfile::Blender &&
              tracked->value.blender.editor == BlenderEditor::Nodes);
        CHECK(tracked->displayedState().appProfile == AppProfile::Zed &&
              status().label == "NORMAL");
        CHECK(key_display(KeyId::U)->hint == normal_hint);
        auto link = ParseBlenderMessage(
                        "KC_STATE 1 token 123 addon 2 2 OBJECT GeometryNodeTree 1 1 NODE_OT_link 0")
                        ->observation;
        link.current = true;
        TestPlatform::status.blenderObservation = link;
        CHECK(!pump()); // Tracking-only updates must not request a GPU frame while locked.
        CHECK(tracked->value.blender.operation == BlenderOperation::NodeLink);
        CHECK(status().label == "NORMAL");
        TestPlatform::pointer.inside = false;
        CHECK(pump());
        CHECK(!tracked->presentation.hoverLocked && status().label == "LINK");
        CHECK(tracked->displayedState().appProfile == AppProfile::Blender);
        CHECK(key_display(KeyId::U)->hint == NO_HINT);
        const auto option_hint = key_display(KeyId::Option)->hint;
        CHECK(option_hint != NO_HINT && GetShortcutHint(option_hint).shortHint == "swap");
        // Startup with the pointer already inside captures the first actual state.
        tracked->presentation = {};
        TestPlatform::status.blenderObservation = observed;
        TestPlatform::pointer.inside = true;
        CHECK(pump());
        CHECK(status().label == "GEOMETRY NODES" && tracked->presentation.hoverLocked);
        TestPlatform::pointer.inside = false;
        TestPlatform::status.blenderObservation = {};
        emit({.type = EventType::ObjectBaseline});
        CHECK(pump());
        TestPlatform::status.description = "Pipeline fixture";
    }
} // namespace

int main()
{
    cmd_registry.add<CommandInitialiseKeyboard>();
    cmd_registry.executeAll();
    const auto keys = registry.getTag(TagName::Key).size();
    CHECK(keys >= 60);
    for (const auto& component : registry.components)
    {
        CHECK(component != nullptr);
    }
    auto& state = registry.getComponent<Tracked_State>(Global::trackerEID)->value.vim;
    const auto feed = [&](std::u32string_view sequence)
    {
        for (auto c : sequence)
        {
            auto event = Key(c);
            event.foregroundGeneration = TestPlatform::status.foregroundGeneration;
            event.lossGeneration = TestPlatform::status.lossGeneration;
            TestPlatform::events.push_back(event);
        }
        UpdateInputInfo();
        cmd_registry.executeAll();
        cmd_registry.add<CommandProjectKeyboard>();
        cmd_registry.executeAll();
    };
    feed(U"dfa");
    CHECK(state.mode == VimMode::Normal && !state.operatorKey &&
          state.waiting == WaitingKind::None && state.confidence != Confidence::Uncertain);
    feed(U"ciw");
    CHECK(state.mode == VimMode::Insert && !state.operatorKey &&
          state.waiting == WaitingKind::None);
    Entity u = -1, d = -1, bracket = -1;
    for (auto entity : registry.getTag(TagName::Key))
    {
        auto key = registry.getComponent<Key_Values>(entity)->key;
        if (key == Platform::KeyId::U)
        {
            u = entity;
        }
        if (key == Platform::KeyId::D)
        {
            d = entity;
        }
        if (key == Platform::KeyId::LeftBracket)
        {
            bracket = entity;
        }
    }
    CHECK(u >= 0 && d >= 0 && bracket >= 0);
    CHECK(!(registry.getComponent<Key_Display>(u)->availableCombinations & 1u));
    CHECK(registry.getComponent<Key_Display>(u)->availableCombinations & (1u << Platform::CONTROL));
    CHECK(registry.getComponent<Key_Display>(u)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(u)->availableModifiers == Platform::CONTROL);
    CHECK(registry.getComponent<Key_Display>(bracket)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(bracket)->availableModifiers == Platform::CONTROL);
    const auto modifiers = [&](std::uint8_t held, bool caps = false)
    {
        TestPlatform::events.push_back(
            Platform::InputEvent{.type = Platform::EventType::Modifiers,
                                 .modifiers = held,
                                 .capsLock = caps,
                                 .foregroundGeneration = TestPlatform::status.foregroundGeneration,
                                 .lossGeneration = TestPlatform::status.lossGeneration});
        feed(U"");
    };
    modifiers(Platform::CONTROL);
    const auto control_hint = registry.getComponent<Key_Display>(u)->hint;
    CHECK(control_hint != NO_HINT && VimBindings()[control_hint].shortHint == "line");
    CHECK(VimBindings()[control_hint].modifiers == Platform::CONTROL);
    const auto bracket_hint = registry.getComponent<Key_Display>(bracket)->hint;
    CHECK(bracket_hint != NO_HINT && VimBindings()[bracket_hint].chord == "Ctrl-[");
    CHECK(!(registry.getComponent<Key_Display>(u)->availableModifiers & Platform::SHIFT));
    modifiers(Platform::CONTROL | Platform::SHIFT);
    CHECK(registry.getComponent<Key_Display>(u)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(u)->availableModifiers == 0);
    CHECK(registry.getComponent<Key_Display>(bracket)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(bracket)->availableModifiers == 0);
    modifiers(Platform::SHIFT);
    CHECK(registry.getComponent<Key_Display>(u)->hint == NO_HINT);
    CHECK(!(registry.getComponent<Key_Display>(u)->availableModifiers & Platform::CONTROL));
    modifiers(0);
    CHECK(registry.getComponent<Key_Display>(u)->hint == NO_HINT);
    Entity escape = -1;
    for (auto entity : registry.getTag(TagName::Key))
    {
        if (registry.getComponent<Key_Values>(entity)->values[0].special ==
            Platform::SpecialKey::Escape)
        {
            escape = entity;
        }
    }
    CHECK(escape >= 0);
    CHECK(KeyChord(*registry.getComponent<Key_Values>(escape), 0) == "Esc");
    modifiers(0, true);
    CHECK(state.mode == VimMode::Insert); // Caps Lock alone does not send Escape.
    modifiers(0);
    auto escape_down = Esc(1);
    escape_down.foregroundGeneration = TestPlatform::status.foregroundGeneration;
    escape_down.lossGeneration = TestPlatform::status.lossGeneration;
    TestPlatform::events.push_back(escape_down);
    feed(U"");
    CHECK(state.mode == VimMode::Normal && registry.getComponent<Key_Display>(escape)->pressed);
    auto escape_up = escape_down;
    escape_up.type = Platform::EventType::KeyUp;
    TestPlatform::events.push_back(escape_up);
    feed(U"");
    CHECK(!registry.getComponent<Key_Display>(escape)->pressed);
    auto normal = Platform::InputEvent{.type = Platform::EventType::NormalBaseline};
    TestPlatform::events.push_back(normal);
    feed(U"");
    const auto* display = registry.getComponent<Key_Display>(d);
    CHECK(display->hint != NO_HINT && VimBindings()[display->hint].shortHint == "del");
    CHECK(display->availableCombinations & (1u << Platform::SHIFT));
    modifiers(0, true);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(d)->hint].value.character == U'D');
    modifiers(Platform::SHIFT, true);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(d)->hint].value.character == U'd');
    modifiers(0);
    TestPlatform::events.push_back(
        Key(U'i')); // Old foreground generation is rejected for Vim tracking.
    feed(U"");
    CHECK(state.mode == VimMode::Normal);
    feed(U"d");
    CHECK(state.operatorKey == U'd');
    ++TestPlatform::status.lossGeneration;
    auto old = Key(U'w');
    old.foregroundGeneration = 1;
    old.lossGeneration = 0;
    TestPlatform::events.push_back(old);
    feed(U"");
    CHECK(state.confidence == Confidence::Uncertain && !state.operatorKey);
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(state.mode == VimMode::Normal && state.confidence == Confidence::Assumed);
    feed(U"i");
    TestPlatform::status.foreground = false;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(state.confidence == Confidence::Uncertain);
    CHECK(Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::I)]);
    const auto inactive_revision = state.revision;
    auto background =
        Platform::InputEvent{.key = Platform::KeyId::V,
                             .modifiers = Platform::SHIFT,
                             .capsLock = true,
                             .foregroundGeneration = TestPlatform::status.foregroundGeneration,
                             .lossGeneration = TestPlatform::status.lossGeneration};
    TestPlatform::events.push_back(background);
    feed(U"");
    CHECK(Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::V)]);
    CHECK(Global::Input::modifiers == Platform::SHIFT && Global::Input::capsLock);
    CHECK(state.revision == inactive_revision && state.mode == VimMode::Insert &&
          !state.operatorKey);
    // Outside supported editors only AeroSpace hints remain; none of them is Shift-only.
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* inactive = registry.getComponent<Key_Display>(entity);
        CHECK(inactive->hint == NO_HINT &&
              (inactive->availableModifiers & ~(Platform::OPTION | Platform::SHIFT)) == 0);
    }
    // A release queued just before an app switch still clears physical feedback.
    auto old_release = background;
    old_release.type = Platform::EventType::KeyUp;
    old_release.key = Platform::KeyId::I;
    --old_release.foregroundGeneration;
    TestPlatform::events.push_back(old_release);
    feed(U"");
    CHECK(!Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::I)] &&
          state.revision == inactive_revision);
    auto inactive_escape = Esc(3);
    inactive_escape.foregroundGeneration = TestPlatform::status.foregroundGeneration;
    inactive_escape.lossGeneration = TestPlatform::status.lossGeneration;
    TestPlatform::events.push_back(inactive_escape);
    feed(U"");
    CHECK(registry.getComponent<Key_Display>(escape)->pressed &&
          registry.getComponent<Key_Display>(escape)->hint == NO_HINT);
    CHECK(state.mode == VimMode::Insert && state.revision == inactive_revision);
    ++TestPlatform::status.lossGeneration;
    feed(U"");
    CHECK(!registry.getComponent<Key_Display>(escape)->pressed && !Global::Input::modifiers &&
          !Global::Input::capsLock);
    CHECK(!Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::V)]);
    TestPlatform::events.push_back(background);
    feed(U""); // Lost-generation events cannot relight keys.
    CHECK(!Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::V)]);
    background.lossGeneration = TestPlatform::status.lossGeneration;
    TestPlatform::status.paused = true;
    TestPlatform::events.push_back(background);
    feed(U"");
    CHECK(!Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::V)]);
    TestPlatform::status.paused = false;
    feed(U"");
    TestPlatform::status.foreground = true;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(state.mode == VimMode::Normal);
    TestPlatform::events.push_back(normal);
    feed(U"");
    CHECK(!(registry.getComponent<Key_Display>(d)->availableCombinations &
            (1u << (Platform::SHIFT | Platform::CONTROL))));
    const auto canonical = registry.getComponent<Key_Layout>(d)->pos;
    cmd_registry.add<CommandProjectKeyboard>(d, 140);
    cmd_registry.executeAll();
    const auto expanded = registry.getComponent<Entity_Size>(d)->size;
    const auto position = registry.getComponent<Entity_Position>(d)->pos;
    CHECK(expanded.x == 140 && position.y == canonical.y);
    CHECK(position.x + expanded.x / 2 ==
          canonical.x + registry.getComponent<Key_Layout>(d)->size.x / 2);
    feed(U"iv"); // Ordinary Insert letters do not select Visual.
    CHECK(state.mode == VimMode::Insert);
    auto literal = Key(U'v', 0, Platform::CONTROL);
    literal.foregroundGeneration = TestPlatform::status.foregroundGeneration;
    literal.lossGeneration = TestPlatform::status.lossGeneration;
    TestPlatform::events.push_back(literal);
    feed(U"");
    const auto escape_hint = registry.getComponent<Key_Display>(escape)->hint;
    CHECK(escape_hint != NO_HINT && VimBindings()[escape_hint].shortHint == "literal");
    // Every edge key and modifier slot must fit, including at the minimum preview size.
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* layout = registry.getComponent<Key_Layout>(entity);
        const Rectangle key{layout->pos.x, layout->pos.y, layout->size.x, layout->size.y};
        for (int slots : {4, 5})
        {
            Rectangle previous{};
            for (int i = 0; i < slots; ++i)
            {
                const auto box = ModifierIconBox(key, i, slots);
                CHECK(box.x >= key.x && box.x + box.width <= key.x + key.width);
                CHECK(box.y >= key.y && box.y + box.height <= key.y + key.height);
                CHECK(i == 0 || box.x >= previous.x + previous.width + 1);
                previous = box;
            }
        }
        cmd_registry.add<CommandProjectKeyboard>(entity, Default::MAX_KEY_WIDTH);
        cmd_registry.executeAll();
        const auto position = registry.getComponent<Entity_Position>(entity)->pos;
        const auto size = registry.getComponent<Entity_Size>(entity)->size;
        CHECK(position.x >= Default::KEYBOARD_EDGE_MARGIN - 0.001f);
        CHECK(position.x + size.x <=
              KeyboardCanvasSize().x - Default::KEYBOARD_EDGE_MARGIN + 0.001f);
        for (const auto window_size : {KeyboardCanvasSize(), Vector2{400, 160}})
        {
            const auto viewport = KeyboardViewport(window_size);
            for (const auto anchor :
                 {Vector2{viewport.x, viewport.y},
                  Vector2{viewport.x + viewport.width, viewport.y + viewport.height}})
            {
                const Camera2D camera{anchor,
                                      {key.x + key.width / 2, key.y + key.height / 2},
                                      0,
                                      Default::HOVER_ZOOM};
                const auto fitted =
                    KeepKeyInViewport(camera, {position.x, position.y, size.x, size.y}, viewport);
                const auto start = GetWorldToScreen2D(position, fitted);
                const auto end =
                    GetWorldToScreen2D({position.x + size.x, position.y + size.y}, fitted);
                CHECK(start.x >= viewport.x - 0.001f && start.y >= viewport.y - 0.001f);
                CHECK(end.x <= viewport.x + viewport.width + 0.001f &&
                      end.y <= viewport.y + viewport.height + 0.001f);
            }
        }
    }
    const auto fitted =
        KeepKeyInViewport({{0, 0}, {0, 0}, 0, 2}, {0, 0, 500, 43}, KeyboardViewport({400, 160}));
    CHECK(std::abs(fitted.zoom - 384.0f / 500) < 0.001f);
    CHECK(registry.getTag(TagName::Key).size() == keys);
    CHECK(registry.getComponent<Entity_Size>(d)->size.x > 0);
    // CPU-only font metrics; no texture is allocated or drawn by these tests.
    std::array<GlyphInfo, 95> glyphs{};
    std::array<Rectangle, 95> recs{};
    for (std::size_t i = 0; i < glyphs.size(); ++i)
    {
        glyphs[i].value = 32 + static_cast<int>(i);
        glyphs[i].advanceX = 10;
        recs[i].width = 10;
    }
    Font font{};
    font.baseSize = 10;
    font.glyphCount = static_cast<int>(glyphs.size());
    font.texture.id = 1;
    font.glyphs = glyphs.data();
    font.recs = recs.data();
    glyphs[U'n' - 32].advanceX = 6;
    glyphs[U'o' - 32].advanceX = 8;
    glyphs[U'r' - 32].advanceX = 7;
    CHECK(TextAdvance(font, "", 10) == 0);
    CHECK(TextAdvance(font, "n", 10) ==
          7); // "o" begins after advance + the missing inter-letter space.
    CHECK(TextAdvance(font, "no", 10) == 16);
    CHECK(TextAdvance(font, "nor", 5) == 13.5f);
    glyphs[U'n' - 32].advanceX = 0;
    glyphs[U'n' - 32].offsetX = 3;
    recs[U'n' - 32].width = 6;
    CHECK(TextAdvance(font, "n", 10) ==
          7); // Match DrawTextEx's zero-advance fallback, not its bounds measurement.
    const auto lines = WrapLabel(font, "delete whole word\nline", 10, 70);
    CHECK(lines == std::vector<std::string>({"delete", "whole", "word", "line"}));
    const auto chunks = WrapLabel(font, "abcdefghij", 10, 40);
    CHECK(chunks == std::vector<std::string>({"abc", "def", "ghi", "j"}));
    CHECK(FitLabel(font, "abcdef", 10, 40) == "a..");
    CHECK(FitLabel(font, "abcdef", 10, 1).empty());
    for (const auto& binding : VimBindings())
    {
        const auto wrapped = WrapLabel(font, binding.fullHint, 11, Default::MAX_KEY_WIDTH - 12);
        for (const auto& line : wrapped)
        {
            CHECK(LabelWidth(font, line, 11) <= Default::MAX_KEY_WIDTH - 12);
        }
        CHECK(40 + 13 * static_cast<float>(wrapped.size() - 1) <= Default::MAX_KEY_HEIGHT);
    }
    Global::Render::font = font;
    const auto find_key = [&](Platform::KeyId key)
    {
        for (auto entity : registry.getTag(TagName::Key))
        {
            if (registry.getComponent<Key_Values>(entity)->key == key)
            {
                return entity;
            }
        }
        return Entity{-1};
    };
    const auto hover = [&](Vector2 point)
    {
        Global::Input::pointer = {point.x, point.y, true};
        TickOnce();
        cmd_registry.executeAll();
        CameraLogic();
    };
    S::reduceMotion = true;
    const auto space = find_key(Platform::KeyId::Space);
    const auto* space_layout = registry.getComponent<Key_Layout>(space);
    CHECK(space >= 0);
    cmd_registry.add<CommandProjectKeyboard>(space, Default::MAX_KEY_WIDTH,
                                             Default::MAX_KEY_HEIGHT);
    cmd_registry.executeAll();
    CHECK(registry.getComponent<Entity_Position>(space)->pos.x == space_layout->pos.x);
    CHECK(registry.getComponent<Entity_Size>(space)->size.x == space_layout->size.x);
    CHECK(registry.getComponent<Entity_Size>(space)->size.y == space_layout->size.y);
    for (const auto target :
         {find_key(Platform::KeyId::RightCommand), find_key(Platform::KeyId::Left)})
    {
        cmd_registry.add<CommandProjectKeyboard>(target, Default::MAX_KEY_WIDTH);
        cmd_registry.executeAll();
        const auto position = registry.getComponent<Entity_Position>(space)->pos;
        CHECK(position.x < space_layout->pos.x && position.y == space_layout->pos.y);
        CHECK(position.x + space_layout->size.x + Default::KEY_GAP <=
              registry.getComponent<Entity_Position>(target)->pos.x);
    }
    for (const auto window_size : {KeyboardCanvasSize(), Vector2{400, 160}})
    {
        S::screenWidth = static_cast<int>(window_size.x);
        S::screenHeight = static_cast<int>(window_size.y);
        for (const auto pair : {std::array{Default::ESCAPE_ON_CAPS_LOCK ? Platform::KeyId::CapsLock
                                                                        : Platform::KeyId::Escape,
                                           Default::ESCAPE_ON_CAPS_LOCK ? Platform::KeyId::A
                                                                        : Platform::KeyId::Grave},
                                std::array{Platform::KeyId::Backspace, Platform::KeyId::Equal},
                                std::array{Platform::KeyId::Right, Platform::KeyId::Down}})
        {
            Global::Render::hoveredEID = -1;
            Global::Render::hoverAmount = 0;
            Global::Render::expandedWidth = 0;
            Global::Render::expandedHeight = 0;
            cmd_registry.add<CommandProjectKeyboard>();
            cmd_registry.executeAll();
            CameraLogic();
            const auto base_camera = Global::Render::camera;
            const auto edge = find_key(pair[0]), neighbor = find_key(pair[1]);
            CHECK(edge >= 0 && neighbor >= 0);
            const auto* layout = registry.getComponent<Key_Layout>(edge);
            const auto* next_layout = registry.getComponent<Key_Layout>(neighbor);
            const auto center = [&](const Key_Layout& key)
            {
                return GetWorldToScreen2D({key.pos.x + key.size.x / 2, key.pos.y + key.size.y / 2},
                                          base_camera);
            };
            hover(center(*layout));
            CHECK(Global::Render::hoveredEID == edge);
            Global::Render::expandedWidth = Default::MAX_KEY_WIDTH;
            cmd_registry.add<CommandProjectKeyboard>(edge, Default::MAX_KEY_WIDTH);
            cmd_registry.executeAll();
            CameraLogic();
            const auto position = registry.getComponent<Entity_Position>(edge)->pos;
            const auto size = registry.getComponent<Entity_Size>(edge)->size;
            const auto neighbor_point = center(*next_layout);
            CHECK(CheckCollisionPointRec(GetScreenToWorld2D(neighbor_point, Global::Render::camera),
                                         {position.x, position.y, size.x, size.y}));
            hover(neighbor_point);
            CHECK(Global::Render::hoveredEID == neighbor);
            const auto gap =
                GetWorldToScreen2D({layout->pos.x + layout->size.x + Default::KEY_GAP / 2,
                                    layout->pos.y + layout->size.y / 2},
                                   base_camera);
            hover(gap);
            CHECK(Global::Render::hoveredEID == -1);
        }
        const auto base_camera = KeyboardBaseCamera(window_size);
        const auto center = GetWorldToScreen2D({space_layout->pos.x + space_layout->size.x / 2,
                                                space_layout->pos.y + space_layout->size.y / 2},
                                               base_camera);
        hover(center);
        CHECK(Global::Render::hoveredEID == space && Global::Render::hoverAmount == 1);
        CHECK(Global::Render::camera.zoom > base_camera.zoom);
        const auto rect = KeyScreenRectangle(
            Global::Render::camera,
            {space_layout->pos.x, space_layout->pos.y, space_layout->size.x, space_layout->size.y});
        const auto viewport = KeyboardViewport(window_size);
        CHECK(rect.x >= viewport.x - 0.001f &&
              rect.x + rect.width <= viewport.x + viewport.width + 0.001f);
        CHECK(rect.y >= viewport.y - 0.001f &&
              rect.y + rect.height <= viewport.y + viewport.height + 0.001f);
        hover(center);
        CHECK(Global::Render::hoveredEID == space);
    }
    Global::Render::font = {};
    // A Zed-to-Ghostty switch selects its profile before interpreting this frame.
    TestPlatform::events.push_back(normal);
    feed(U"d");
    CHECK(state.operatorKey == U'd');
    auto stale_zed = Key(U'v', 0, Platform::CONTROL);
    stale_zed.foregroundGeneration = TestPlatform::status.foregroundGeneration;
    stale_zed.lossGeneration = TestPlatform::status.lossGeneration;
    ++TestPlatform::status.foregroundGeneration;
    TestPlatform::status.appProfile = Platform::AppProfile::Neovim;
    TestPlatform::events.push_back(stale_zed);
    feed(U"i");
    CHECK(state.appProfile == Platform::AppProfile::Neovim && state.mode == VimMode::Insert);
    CHECK(state.waiting == WaitingKind::None && !state.operatorKey && state.pendingLength == 0);
    const auto b = find_key(Platform::KeyId::B);
    const auto y = find_key(Platform::KeyId::Y);
    CHECK(!(registry.getComponent<Key_Display>(b)->availableModifiers & Platform::CONTROL));
    CHECK(registry.getComponent<Key_Display>(d)->availableModifiers & Platform::CONTROL);
    modifiers(Platform::CONTROL);
    CHECK(registry.getComponent<Key_Display>(b)->hint == NO_HINT);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(d)->hint].fullHint == "decrease indent");
    TestPlatform::events.push_back(normal);
    modifiers(Platform::SHIFT);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(y)->hint].fullHint ==
          "yank to line end");
    modifiers(0);
    feed(U"d");
    auto ghostty_tab =
        Platform::InputEvent{.key = Platform::KeyId::Tab,
                             .value = {0, Platform::SpecialKey::Tab},
                             .modifiers = Platform::CONTROL,
                             .foregroundGeneration = TestPlatform::status.foregroundGeneration,
                             .lossGeneration = TestPlatform::status.lossGeneration};
    TestPlatform::events.push_back(ghostty_tab);
    feed(U"");
    CHECK(!state.operatorKey && state.mode == VimMode::Normal &&
          state.confidence == Confidence::Assumed);
    feed(U".");
    CHECK(state.confidence == Confidence::Uncertain);
    feed(U"i");
    modifiers(Platform::CONTROL);
    CHECK(Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::I)]);
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* resync = registry.getComponent<Key_Display>(entity);
        CHECK(resync->hint == NO_HINT && resync->availableCombinations == 0 &&
              resync->availableModifiers == 0);
    }
    CHECK(Global::Input::modifiers == Platform::CONTROL &&
          registry.getComponent<Key_Display>(escape)->hint == NO_HINT);
    TestPlatform::events.push_back(normal);
    modifiers(0);
    feed(U"d");
    CHECK(registry.getComponent<Key_Display>(d)->hint != NO_HINT);
    // Profile changes clear continuations even if a preview/test backend reuses a generation.
    TestPlatform::status.appProfile = Platform::AppProfile::Zed;
    feed(U"i");
    CHECK(state.appProfile == Platform::AppProfile::Zed && state.mode == VimMode::Insert &&
          !state.operatorKey);
    CHECK(registry.getComponent<Key_Display>(b)->availableModifiers & Platform::CONTROL);
    feed(U"."); // Ordinary Insert input remains ordinary after the switch back.
    CHECK(state.confidence != Confidence::Uncertain);
    const auto i = find_key(Platform::KeyId::I);
    const auto a = find_key(Platform::KeyId::A);
    const auto q = find_key(Platform::KeyId::Q);
    const auto emit = [&](Platform::InputEvent event)
    {
        event.foregroundGeneration = TestPlatform::status.foregroundGeneration;
        event.lossGeneration = TestPlatform::status.lossGeneration;
        TestPlatform::events.push_back(event);
    };
    TestPlatform::events.push_back(normal);
    emit(Key(U'v', 0, Platform::CONTROL));
    feed(U"jjj");
    CHECK(state.mode == VimMode::VisualBlock && state.confidence != Confidence::Uncertain);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(i)->hint].shortHint == "inner");
    CHECK(registry.getComponent<Key_Display>(i)->availableModifiers & Platform::SHIFT);
    modifiers(Platform::SHIFT);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(i)->hint].fullHint ==
          "insert at the start of each block row");
    CHECK(VimBindings()[registry.getComponent<Key_Display>(a)->hint].fullHint ==
          "append at the end of each block row");
    feed(U"Ityped");
    CHECK(state.mode == VimMode::Insert && !state.pendingLength &&
          state.confidence != Confidence::Uncertain);
    emit(Esc(30));
    feed(U"");
    CHECK(state.mode == VimMode::Normal);
    TestPlatform::events.push_back(normal);
    feed(U"glgItyped");
    CHECK(state.mode == VimMode::Insert && state.confidence != Confidence::Uncertain &&
          !state.pendingLength);
    TestPlatform::events.push_back(normal);
    feed(U"Vjg");
    modifiers(Platform::SHIFT);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(a)->hint].chord == "gA");
    feed(U"A");
    CHECK(state.mode == VimMode::Insert && !state.pendingLength);
    TestPlatform::status.appProfile = Platform::AppProfile::Neovim;
    feed(U":w");
    CHECK(state.context == VimContext::CommandLine &&
          std::string_view(state.pendingSequence.data()) == ":w");
    const auto ex_hint = registry.getComponent<Key_Display>(q)->hint;
    CHECK(ex_hint != NO_HINT && VimBindings()[ex_hint].chord == ":wq");
    CHECK(HintChord(*registry.getComponent<Key_Values>(q), GetShortcutHint(ex_hint)) == "q -> :wq");
    CHECK(registry.getComponent<Key_Display>(i)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(u)->hint == NO_HINT);
    CHECK(registry.getComponent<Key_Display>(u)->availableModifiers & Platform::CONTROL);
    modifiers(Platform::CONTROL);
    CHECK(VimBindings()[registry.getComponent<Key_Display>(u)->hint].chord == "Ctrl-u");
    CHECK(registry.getComponent<Key_Display>(q)->hint == NO_HINT);
    emit(Key(U'u', 0, Platform::CONTROL));
    feed(U"w");
    emit({.value = {0, Platform::SpecialKey::Enter}});
    feed(U"");
    CHECK(state.context == VimContext::Editor && state.mode == VimMode::Normal &&
          state.confidence == Confidence::Inferred);
    feed(U":wq");
    emit({.value = {0, Platform::SpecialKey::Enter}});
    feed(U"");
    CHECK(state.confidence == Confidence::Uncertain);
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* resync = registry.getComponent<Key_Display>(entity);
        CHECK(resync->hint == NO_HINT && !resync->availableCombinations &&
              !resync->availableModifiers);
    }
    TestPlatform::status.appProfile = Platform::AppProfile::Zed;
    feed(U"");
    TestPlatform::events.push_back(normal);
    feed(U".");
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* resync = registry.getComponent<Key_Display>(entity);
        CHECK(resync->hint == NO_HINT && !resync->availableCombinations &&
              !resync->availableModifiers);
    }
    auto& app_state = registry.getComponent<Tracked_State>(Global::trackerEID)->value;
    auto& blender = app_state.blender;
    TestPlatform::status.appProfile = Platform::AppProfile::Blender;
    TestPlatform::status.macCommandAliases = true;
    ++TestPlatform::status.foregroundGeneration;
    modifiers(0);
    feed(U"g");
    CHECK(app_state.appProfile == Platform::AppProfile::Blender &&
          blender.operation == BlenderOperation::Move);
    CHECK(GetShortcutHint(registry.getComponent<Key_Display>(find_key(Platform::KeyId::X))->hint)
              .shortHint == "axis");
    CHECK(registry.getComponent<Key_Display>(find_key(Platform::KeyId::Control))->hint != NO_HINT);
    emit(Esc(40));
    feed(U"");
    CHECK(blender.mode == BlenderMode::Object && blender.operation == BlenderOperation::None);
    CHECK(registry.getComponent<Key_Display>(find_key(Platform::KeyId::E))->hint == NO_HINT);
    emit({.type = Platform::EventType::MeshEditBaseline});
    feed(U"");
    CHECK(GetShortcutHint(registry.getComponent<Key_Display>(find_key(Platform::KeyId::E))->hint)
              .shortHint == "extr");
    // Caps Lock and Option-generated Unicode must not change Blender's physical shortcut matching.
    modifiers(0, true);
    feed(U"i");
    CHECK(blender.operation == BlenderOperation::Inset);
    emit(Esc(41));
    feed(U"");
    modifiers(Platform::COMMAND);
    CHECK(GetShortcutHint(registry.getComponent<Key_Display>(b)->hint).shortHint == "bevel");
    emit(Key(U'b', 0, Platform::COMMAND));
    feed(U"");
    CHECK(blender.operation == BlenderOperation::Bevel);
    emit(Esc(42));
    feed(U"");
    modifiers(0);
    feed(U"g");
    auto stale =
        Platform::InputEvent{.key = Platform::KeyId::Tab,
                             .value = {0, Platform::SpecialKey::Tab},
                             .foregroundGeneration = TestPlatform::status.foregroundGeneration - 1,
                             .lossGeneration = TestPlatform::status.lossGeneration};
    TestPlatform::events.push_back(stale);
    feed(U"");
    CHECK(blender.operation == BlenderOperation::Move);
    emit({.type = Platform::EventType::MouseAction, .mouseButton = Platform::MouseButton::Left});
    feed(U"");
    CHECK(blender.operation == BlenderOperation::None && blender.mode == BlenderMode::MeshEdit);
    TestPlatform::status.foreground = false;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(blender.confidence != Confidence::Uncertain);
    TestPlatform::status.foreground = true;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(blender.mode == BlenderMode::MeshEdit && blender.confidence == Confidence::Assumed);
    feed(U"g");
    TestPlatform::status.foreground = false;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(blender.confidence == Confidence::Uncertain);
    TestPlatform::status.foreground = true;
    ++TestPlatform::status.foregroundGeneration;
    feed(U"");
    CHECK(blender.confidence == Confidence::Uncertain);
    for (auto entity : registry.getTag(TagName::Key))
    {
        CHECK(registry.getComponent<Key_Display>(entity)->hint == NO_HINT);
    }
    emit(Esc(43));
    feed(U"");
    CHECK(blender.mode == BlenderMode::MeshEdit && blender.confidence == Confidence::Assumed);
    CHECK(registry.getComponent<Key_Display>(find_key(Platform::KeyId::G))->hint != NO_HINT);
    feed(U"m");
    emit({.key = Platform::KeyId::Enter, .value = {0, Platform::SpecialKey::Enter}});
    feed(U"");
    CHECK(blender.context == BlenderContext::Editor && blender.confidence == Confidence::Assumed);
    feed(U"z2g1/2");
    CHECK(blender.operation == BlenderOperation::Move && blender.numericInput &&
          blender.confidence != Confidence::Uncertain);
    emit(Esc(43.5));
    feed(U"");
    CHECK(blender.mode == BlenderMode::MeshEdit && blender.operation == BlenderOperation::None);
    emit({.type = Platform::EventType::MeshEditBaseline});
    feed(U"");
    ++TestPlatform::status.lossGeneration;
    feed(U"");
    CHECK(blender.confidence == Confidence::Uncertain);
    emit(Esc(43.8));
    feed(U"");
    CHECK(blender.confidence == Confidence::Uncertain &&
          blender.uncertainty == BlenderUncertainty::Mode);
    emit({.type = Platform::EventType::ObjectBaseline});
    feed(U"");
    CHECK(blender.mode == BlenderMode::Object);
    feed(U"g");
    const auto tracker_id = Global::trackerEID;
    S::showFunctionRow = true;
    S::showNumpad = true;
    cmd_registry.add<CommandRebuildKeyboard>();
    cmd_registry.executeAll();
    cmd_registry.add<CommandProjectKeyboard>();
    cmd_registry.executeAll();
    CHECK(Global::trackerEID == tracker_id && blender.operation == BlenderOperation::Move);
    CHECK(find_key(Platform::KeyId::F3) >= 0 && find_key(Platform::KeyId::NumpadOne) >= 0);
    CHECK(KeyboardCanvasSize().x > Default::SCREEN_WIDTH &&
          KeyboardCanvasSize().y > Default::SCREEN_HEIGHT);
    const auto pad = find_key(Platform::KeyId::NumpadOne);
    CHECK(GetShortcutHint(registry.getComponent<Key_Display>(pad)->hint).shortHint == "value");
    emit({.key = Platform::KeyId::NumpadOne});
    feed(U"");
    CHECK(blender.numericInput);
    emit({.key = Platform::KeyId::NumpadEnter, .value = {0, Platform::SpecialKey::Enter}});
    feed(U"");
    CHECK(GetShortcutHint(registry.getComponent<Key_Display>(pad)->hint).shortHint == "front");
    emit({.key = Platform::KeyId::F3});
    feed(U"");
    CHECK(blender.context == BlenderContext::Search);
    emit(Esc(44));
    feed(U"");
    for (auto entity : registry.getTag(TagName::Key))
    {
        const auto* layout = registry.getComponent<Key_Layout>(entity);
        CHECK(layout->pos.x + layout->size.x <=
              KeyboardCanvasSize().x - Default::KEYBOARD_EDGE_MARGIN);
        CHECK(layout->pos.y + layout->size.y <=
              KeyboardCanvasSize().y - Default::KEYBOARD_EDGE_MARGIN);
    }
    S::showFunctionRow = false;
    S::showNumpad = false;
    cmd_registry.add<CommandRebuildKeyboard>();
    cmd_registry.executeAll();
    CHECK(registry.getTag(TagName::Key).size() == keys && blender.mode == BlenderMode::Object);

    // Real state reports follow key commands, without changing physical input feedback.
    auto observed =
        Platform::ParseBlenderMessage("KC_STATE 1 token 123 addon 1 1 EDIT_MESH VIEW_3D 4 1 NONE 0")
            ->observation;
    observed.current = true;
    observed.inputSequence = 100;
    TestPlatform::status.blenderObservation = observed;
    feed(U"");
    CHECK(blender.mode == BlenderMode::MeshEdit && blender.selectionMask == 4 &&
          blender.confidence == Confidence::Observed);
    CHECK(!UpdateInputInfo());
    cmd_registry.executeAll();
    emit({.key = Platform::KeyId::G});
    TestPlatform::status.blenderObservation.current = false;
    feed(U"");
    CHECK(blender.operation == BlenderOperation::Move);
    TestPlatform::status.blenderObservation.current = true;
    ++TestPlatform::status.blenderObservation.inputSequence;
    feed(U"");
    CHECK(blender.operation == BlenderOperation::None &&
          blender.confidence == Confidence::Observed);
    auto curve_observation = Platform::ParseBlenderMessage(
                                 "KC_STATE 1 token 123 addon 2 2 EDIT_CURVE VIEW_3D 1 1 NONE 0")
                                 ->observation;
    curve_observation.current = true;
    TestPlatform::status.blenderObservation = curve_observation;
    feed(U"");
    CHECK(blender.mode == BlenderMode::CurveEdit && blender.confidence == Confidence::Observed);
    const auto curve_key = [&](Platform::KeyId id)
    {
        const auto entity = find_key(id);
        CHECK(entity >= 0);
        return registry.getComponent<Key_Display>(entity)->hint;
    };
    CHECK(curve_key(Platform::KeyId::E) != NO_HINT);
    CHECK(curve_key(Platform::KeyId::V) != NO_HINT);
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::G)).fullHint.find("control points") !=
          std::string_view::npos);
    CHECK(curve_key(Platform::KeyId::I) == NO_HINT);
    emit({.type = Platform::EventType::Modifiers, .modifiers = Platform::OPTION});
    TestPlatform::status.blenderObservation.current = false;
    feed(U"");
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::S)).shortHint == "radius");
    emit({.type = Platform::EventType::Modifiers});
    feed(U"");
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::S)).shortHint == "scale");
    emit({.key = Platform::KeyId::E});
    feed(U"");
    CHECK(blender.operation == BlenderOperation::Extrude);
    emit({.key = Platform::KeyId::Escape});
    feed(U"");
    CHECK(blender.mode == BlenderMode::CurveEdit && blender.operation == BlenderOperation::None);
    auto node_observation =
        Platform::ParseBlenderMessage(
            "KC_STATE 1 token 123 addon 3 3 EDIT_MESH GeometryNodeTree 1 1 NONE 0")
            ->observation;
    node_observation.current = true;
    TestPlatform::status.blenderObservation = node_observation;
    feed(U"");
    CHECK(blender.editor == BlenderEditor::Nodes && blender.mode == BlenderMode::MeshEdit);
    CHECK(curve_key(Platform::KeyId::E) == NO_HINT);
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::Shift)).shortHint == "reroute");
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::Control)).shortHint == "cut links");
    emit({.type = Platform::EventType::Modifiers,
          .modifiers = Platform::CONTROL | Platform::OPTION});
    TestPlatform::status.blenderObservation.current = false;
    feed(U"");
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::Control)).shortHint == "mute links");
    CHECK(GetShortcutHint(curve_key(Platform::KeyId::Option)).shortHint == "lasso");
    emit({.type = Platform::EventType::Modifiers});
    emit({.key = Platform::KeyId::Tab});
    feed(U"");
    CHECK(blender.editor == BlenderEditor::Nodes && blender.mode == BlenderMode::MeshEdit);
    emit({.type = Platform::EventType::MouseAction,
          .modifiers = Platform::SHIFT,
          .mouseButton = Platform::MouseButton::Right});
    feed(U"");
    CHECK(blender.operation == BlenderOperation::NodeReroute && !blender.popupPending);
    emit({.type = Platform::EventType::MouseAction,
          .mouseButton = Platform::MouseButton::Right,
          .mousePressed = false});
    feed(U"");
    CHECK(blender.operation == BlenderOperation::None && curve_key(Platform::KeyId::G) != NO_HINT);
    node_observation = Platform::ParseBlenderMessage(
                           "KC_STATE 1 token 123 addon 4 4 OBJECT NODE_EDITOR_UI 1 1 NONE 0")
                           ->observation;
    node_observation.current = true;
    TestPlatform::status.blenderObservation = node_observation;
    feed(U"");
    for (auto entity : registry.getTag(TagName::Key))
    {
        CHECK(registry.getComponent<Key_Display>(entity)->hint == NO_HINT);
    }
    const auto sculpt_observation =
        Platform::ParseBlenderMessage("KC_STATE 1 token 123 addon 2 2 SCULPT VIEW_3D 4 1 NONE 0")
            ->observation;
    TestPlatform::status.blenderObservation = sculpt_observation;
    TestPlatform::status.blenderObservation.current = true;
    feed(U"");
    CHECK(blender.mode == BlenderMode::Unsupported);
    for (auto entity : registry.getTag(TagName::Key))
    {
        CHECK(registry.getComponent<Key_Display>(entity)->hint == NO_HINT);
    }
    TestPlatform::status.preview = true;
    TestPlatform::status.blenderObservation = observed;
    feed(U"");
    CHECK(blender.mode == BlenderMode::Unsupported); // A live bridge never drives local preview.
    TestPlatform::status.preview = false;
    ++TestPlatform::status.blenderObservation.inputSequence;
    feed(U"");
    CHECK(blender.mode == BlenderMode::MeshEdit);
    TestPlatform::status.blenderObservation = {};
    feed(U"");
    CHECK(blender.confidence == Confidence::Uncertain && !blender.observation.connected);
    emit({.type = Platform::EventType::ObjectBaseline});
    feed(U"");
    CHECK(blender.mode == BlenderMode::Object && blender.confidence == Confidence::Assumed);

    TestHoverPresentation();
    TestFunctionLayer();
    TestAeroSpace();

    // Idle status and movement outside the overlay must not request a new GPU frame.
    ResetInputInfo();
    CHECK(UpdateInputInfo());
    cmd_registry.executeAll();
    CHECK(!UpdateInputInfo());
    TestPlatform::pointer = {100, 200, false};
    CHECK(!UpdateInputInfo());
    TestPlatform::pointer.inside = true;
    CHECK(UpdateInputInfo());
    CHECK(!UpdateInputInfo());
    TestPlatform::pointer.x += 1;
    CHECK(UpdateInputInfo());
    TestPlatform::pointer.inside = false;
    CHECK(UpdateInputInfo());
    CHECK(!UpdateInputInfo());
    TestPlatform::status.reduceMotion = !TestPlatform::status.reduceMotion;
    CHECK(UpdateInputInfo());
    CHECK(!UpdateInputInfo());
    ++TestPlatform::status.lossGeneration;
    CHECK(UpdateInputInfo());
    cmd_registry.executeAll();
    CHECK(!UpdateInputInfo());

    // Hover animations must finish exactly, allowing the event loop to sleep.
    S::reduceMotion = false;
    Global::Input::status.reduceMotion = false;
    Global::dt = 1.0f / Default::TARGET_FPS;
    const auto* resting_key = registry.getComponent<Key_Layout>(find_key(Platform::KeyId::E));
    const auto resting_center = Vector2{resting_key->pos.x + resting_key->size.x / 2,
                                        resting_key->pos.y + resting_key->size.y / 2};
    const auto screen_center = GetWorldToScreen2D(
        resting_center, KeyboardBaseCamera({static_cast<float>(S::screenWidth),
                                            static_cast<float>(S::screenHeight)}));
    Global::Input::pointer = {screen_center.x, screen_center.y, true};
    for (int i = 0; i < 180; ++i)
    {
        TickOnce();
        cmd_registry.executeAll();
    }
    CHECK(!Global::Render::animating && Global::Render::hoverAmount == 1);
    Global::Input::pointer.inside = false;
    for (int i = 0; i < 180; ++i)
    {
        TickOnce();
        cmd_registry.executeAll();
    }
    CHECK(!Global::Render::animating && Global::Render::hoverAmount == 0);
    CHECK(Global::Render::expandedWidth == 0 && Global::Render::expandedHeight == 0);
    std::cout << "Real input → command → ECS → Vim / Blender hint projection checks passed\n";
}
