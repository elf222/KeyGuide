// gameLogic/command_execution.cpp
#include "registry/commands.hpp"
#include <algorithm>
#include "gameLogic/tracker.hpp"
#include "gameLogic/aerospace_bindings.hpp"
#include "gameLogic/blender_tracker.hpp"
#include "gameWorld/keyboard_layout.hpp"
#include "global/global.hpp"
#include "registry/global.hpp"

void CommandDestroyEntity::execute()
{
    registry.destroyEntity(e);
}

void CommandInitialiseKeyboard::execute()
{
    Global::trackerEID = registry.createEntity();
    registry.addTag(Global::trackerEID, TagName::Tracker);
    registry.emplaceComponent<Tracked_State>(Global::trackerEID);
    CommandRebuildKeyboard{}.execute();
}

void CommandRebuildKeyboard::execute()
{
    const auto old_keys = registry.getTag(TagName::Key);
    for (auto entity : old_keys)
    {
        registry.destroyEntity(entity);
    }
    Global::Render::hoveredEID = -1;
    Global::Render::expandedWidth = 0;
    Global::Render::expandedHeight = 0;
    for (const auto& layout : KeyboardLayout())
    {
        const auto entity = registry.createEntity();
        registry.addTag(entity, TagName::Key);
        registry.emplaceComponent<Entity_Position>(entity, Entity_Position{layout.pos});
        registry.emplaceComponent<Entity_Size>(entity, Entity_Size{layout.size});
        registry.emplaceComponent<Key_Layout>(entity,
                                              Key_Layout{layout.pos, layout.size, layout.row});
        registry.emplaceComponent<Colour>(entity, Default::KEY_BACKGROUND);
        const auto function_key = Platform::FunctionKeyForNumber(layout.key);
        registry.emplaceComponent<Key_Values>(entity,
                                              Key_Values{layout.key, layout.values, function_key});
        auto& hints = registry.emplaceComponent<Key_Hint>(entity);
        for (std::uint8_t modifiers = 0; modifiers < 16; ++modifiers)
        {
            for (HintId i = 0; i < ShortcutCount(); ++i)
            {
                if (ShortcutMatchesKey(i, layout.key, layout.values, modifiers))
                {
                    hints.byModifiers[modifiers].push_back(i);
                }
                if (function_key != Platform::KeyId::Unknown &&
                    ShortcutMatchesKey(i, function_key, {}, modifiers))
                {
                    hints.byFunctionModifiers[modifiers].push_back(i);
                }
            }
        }
        registry.emplaceComponent<Key_Display>(entity);
    }
}

void CommandTrackInput::execute()
{
    auto* state = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (state)
    {
        TrackInput(state->value, event);
    }
}

void CommandSetAppProfile::execute()
{
    auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (!tracked)
    {
        return;
    }
    SetAppProfile(tracked->value, profile);
    tracked->value.blender.macCommandAliases = macCommandAliases;
}

void CommandFocusChanged::execute()
{
    auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (tracked)
    {
        TrackFocusChange(tracked->value, foreground);
    }
}

void CommandObserveBlender::execute()
{
    auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (tracked && tracked->value.appProfile == Platform::AppProfile::Blender)
    {
        ObserveBlenderState(tracked->value.blender, observation);
    }
}

void CommandInvalidateState::execute()
{
    auto* state = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (state)
    {
        InvalidateTrackedState(state->value, reason);
        if (state->value.appProfile == Platform::AppProfile::Blender)
        {
            state->value.blender.observation = {};
        }
    }
}

void CommandSetDisplayHoverLock::execute()
{
    auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (tracked)
    {
        tracked->presentation.hoverLocked = inside && tracked->presentation.initialised;
    }
}

void CommandProjectKeyboard::execute()
{
    auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    if (!tracked)
    {
        return;
    }
    tracked->presentation.hoverLocked =
        Global::Input::pointer.inside && tracked->presentation.initialised;
    if (!tracked->presentation.hoverLocked)
    {
        tracked->presentation.capture(tracked->value, Global::Input::status);
        tracked->presentation.hoverLocked = Global::Input::pointer.inside;
    }
    const auto& state = tracked->displayedState();
    const auto input = tracked->displayedInput(Global::Input::status);
    const bool aerospace = AeroSpaceHintsShown(input);
    const bool show_hints =
        aerospace || (input.foreground && !input.paused && (input.preview || input.available) &&
                      TrackedConfidence(state) != Confidence::Uncertain);
    const bool function_layer =
        (Global::Input::functionHeld && !Global::Input::functionTypesNumbers) ||
        Global::Input::functionKey != Platform::KeyId::Unknown;
    const auto first_available = [&](const std::vector<HintId>& ids)
    {
        const auto found = std::find_if(
            ids.begin(), ids.end(),
            [&](HintId id)
            {
                return aerospace ? AeroSpaceShortcutAvailable(id, Global::Input::aerospaceService)
                                 : ShortcutAvailable(id, state);
            });
        return found == ids.end() ? NO_HINT : *found;
    };
    const auto* hovered_layout = registry.getComponent<Key_Layout>(hovered);
    const auto* hovered_values = registry.getComponent<Key_Values>(hovered);
    const bool status_key = hovered_values && hovered_values->key == Platform::KeyId::Space;
    const auto canvas = KeyboardCanvasSize();
    const float extra =
        hovered_layout && !status_key
            ? std::max(0.0f, std::min(expandedWidth, canvas.x - 2 * Default::KEYBOARD_EDGE_MARGIN) -
                                 hovered_layout->size.x)
            : 0;
    const float left_extra =
        hovered_layout
            ? std::clamp(extra / 2,
                         std::max(0.0f, extra - (canvas.x - Default::KEYBOARD_EDGE_MARGIN -
                                                 hovered_layout->pos.x - hovered_layout->size.x)),
                         std::min(extra, std::max(0.0f, hovered_layout->pos.x -
                                                            Default::KEYBOARD_EDGE_MARGIN)))
            : 0;
    for (auto entity : registry.getTag(TagName::Key))
    {
        auto* display = registry.getComponent<Key_Display>(entity);
        const auto* values = registry.getComponent<Key_Values>(entity);
        const auto* hints = registry.getComponent<Key_Hint>(entity);
        const auto* layout = registry.getComponent<Key_Layout>(entity);
        display->hint = NO_HINT;
        display->availableCombinations = 0;
        display->availableModifiers = 0;
        const bool function_alias = values->functionKey != Platform::KeyId::Unknown;
        display->functionLayer = function_alias && function_layer;
        display->functionAvailable = false;
        display->hovered = entity == hovered;
        display->pressed = Global::Input::pressed[static_cast<std::size_t>(values->key)] ||
                           (function_alias &&
                            Global::Input::pressed[static_cast<std::size_t>(values->functionKey)]);
        if (values->key == Platform::KeyId::CapsLock &&
            values->values[0].special == Platform::SpecialKey::Escape)
        {
            display->pressed |=
                Global::Input::pressed[static_cast<std::size_t>(Platform::KeyId::Escape)];
        }
        const auto caps_shift = (!aerospace && state.appProfile != Platform::AppProfile::Blender &&
                                 Global::Input::capsLock && values->key >= Platform::KeyId::A &&
                                 values->key <= Platform::KeyId::Z)
                                    ? Platform::SHIFT
                                    : 0;
        const std::size_t preferred_modifiers = Global::Input::modifiers ^ caps_shift;
        const auto& combinations =
            display->functionLayer ? hints->byFunctionModifiers : hints->byModifiers;
        for (std::size_t combination = 0; show_hints && combination < combinations.size();
             ++combination)
        {
            const auto id = first_available(combinations[combination]);
            if (function_alias && !display->functionAvailable &&
                (combination & preferred_modifiers) == preferred_modifiers)
            {
                const auto function_id =
                    display->functionLayer
                        ? id
                        : first_available(hints->byFunctionModifiers[combination]);
                display->functionAvailable = function_id != NO_HINT;
            }
            if (id == NO_HINT)
            {
                continue;
            }
            display->availableCombinations |= static_cast<std::uint16_t>(1u << combination);
            if (combination == preferred_modifiers)
            {
                display->hint = id;
            }
            const auto physical_combination = combination ^ caps_shift;
            if ((physical_combination & Global::Input::modifiers) == Global::Input::modifiers)
            {
                display->availableModifiers |= static_cast<std::uint8_t>(physical_combination);
            }
        }
        auto* position = registry.getComponent<Entity_Position>(entity);
        auto* size = registry.getComponent<Entity_Size>(entity);
        position->pos = layout->pos;
        size->size = layout->size;
        if (display->hovered && !status_key)
        {
            position->pos.x -= left_extra;
            size->size.x += extra;
            size->size.y =
                std::max(layout->size.y, std::min(Default::MAX_KEY_HEIGHT, expandedHeight));
        }
        else if (hovered_layout && layout->row == hovered_layout->row)
        {
            position->pos.x +=
                layout->pos.x > hovered_layout->pos.x ? extra - left_extra : -left_extra;
        }
    }
}
