// gameLogic/input.cpp
#include "gameLogic/input.hpp"
#include <array>
#include "gameLogic/aerospace_bindings.hpp"
#include "global/global.hpp"
#include "platform/platform.hpp"
#include "registry/global.hpp"

namespace
{
    Platform::InputBatchInfo previous{};
    bool initial = true;

    void ReleaseKeys()
    {
        Global::Input::pressed.fill(false);
        Global::Input::modifiers = 0;
        Global::Input::capsLock = false;
        Global::Input::functionHeld = false;
        Global::Input::functionKey = Platform::KeyId::Unknown;
        Global::Input::aerospaceService = false;
    }
} // namespace

void ResetInputInfo()
{
    previous = {};
    initial = true;
    ReleaseKeys();
    Global::Input::functionTypesNumbers = false;
}

bool UpdateInputInfo()
{
    std::array<Platform::InputEvent, 512> events{};
    const auto status = Platform::ReadInputEvents(events);
    const auto pointer = Platform::ReadPointerPosition();
    const auto old_pointer = Global::Input::pointer;
    const bool pointer_changed =
        pointer.inside != old_pointer.inside ||
        (pointer.inside && (pointer.x != old_pointer.x || pointer.y != old_pointer.y));
    const auto* tracked = registry.getComponent<Tracked_State>(Global::trackerEID);
    const bool hover_locked = pointer.inside && tracked && tracked->presentation.initialised;
    // Lock the previous presentation before this batch changes the live tracker.
    cmd_registry.add<CommandSetDisplayHoverLock>(pointer.inside);
    const bool profile_changed = initial || status.appProfile != previous.appProfile;
    const bool focus_changed = initial || profile_changed ||
                               status.foregroundGeneration != previous.foregroundGeneration ||
                               status.foreground != previous.foreground ||
                               status.preview != previous.preview;
    const bool lost = !initial && status.lossGeneration != previous.lossGeneration;
    const bool availability_changed =
        !initial && (status.available != previous.available || status.paused != previous.paused);
    const bool changed =
        initial || focus_changed || lost || availability_changed || status.count > 0 ||
        status.reduceMotion != previous.reduceMotion || status.quit != previous.quit ||
        (!hover_locked && status.blenderObservation != previous.blenderObservation);
    if (initial || lost || availability_changed ||
        (focus_changed && (status.preview || previous.preview)))
    {
        ReleaseKeys();
    }
    if (profile_changed || status.macCommandAliases != previous.macCommandAliases)
    {
        cmd_registry.add<CommandSetAppProfile>(status.appProfile, status.macCommandAliases);
    }
    if (focus_changed || lost || availability_changed)
    {
        if (focus_changed && status.appProfile == Platform::AppProfile::Blender && !lost &&
            !availability_changed)
        {
            cmd_registry.add<CommandFocusChanged>(status.foreground);
        }
        else if (focus_changed && status.foreground && !lost && !availability_changed)
        {
            cmd_registry.add<CommandTrackInput>(Platform::InputEvent{
                .type = Platform::EventType::NormalBaseline,
                .baselineReason = status.preview ? "Normal assumed on preview entry"
                                  : status.appProfile == Platform::AppProfile::Neovim
                                      ? "Normal assumed on entry / return to Ghostty; Neovim must "
                                        "be focused inside it"
                                      : "Normal assumed on entry / return to Zed"});
        }
        else
        {
            cmd_registry.add<CommandInvalidateState>(
                lost                 ? "Observation lost; use an application baseline to resync"
                : status.paused      ? "Observation paused"
                : !status.foreground ? "Outside supported editor / preview"
                                     : "Observation unavailable or restarted; resync");
        }
    }
    for (std::size_t i = 0; i < status.count; ++i)
    {
        const auto& event = events[i];
        const bool manual = event.type >= Platform::EventType::NormalBaseline;
        if (!manual &&
            (status.paused || (!status.available && !status.preview) ||
             event.lossGeneration != status.lossGeneration ||
             (status.preview &&
              (!status.foreground || event.foregroundGeneration != status.foregroundGeneration))))
        {
            continue;
        }
        if (!manual)
        {
            const auto key = static_cast<std::size_t>(event.key);
            const bool down = event.type == Platform::EventType::KeyDown ||
                              event.type == Platform::EventType::Unsupported;
            if (key < Global::Input::pressed.size())
            {
                if (down)
                {
                    Global::Input::pressed[key] = true;
                }
                if (event.type == Platform::EventType::KeyUp)
                {
                    Global::Input::pressed[key] = false;
                }
            }
            // AeroSpace hotkeys are global, so its mode follows every app's input.
            if (down && !event.repeat)
            {
                Global::Input::aerospaceService = AeroSpaceServiceAfter(
                    Global::Input::aerospaceService, event.key, event.modifiers);
            }
            if (down && !Platform::ModifierBit(event.key))
            {
                Global::Input::functionKey =
                    event.key >= Platform::KeyId::F1 && event.key <= Platform::KeyId::F12
                        ? event.key
                        : Platform::KeyId::Unknown;
                Global::Input::functionTypesNumbers |=
                    event.functionHeld &&
                    Platform::FunctionKeyForNumber(event.key) != Platform::KeyId::Unknown;
            }
            if (event.type == Platform::EventType::KeyUp && event.key == Global::Input::functionKey)
            {
                Global::Input::functionKey = Platform::KeyId::Unknown;
            }
            Global::Input::modifiers = event.modifiers;
            Global::Input::capsLock = event.capsLock;
            Global::Input::functionHeld = event.functionHeld;
        }
        // Physical feedback survives app switches; only current app/preview input advances tracked
        // state.
        if (!manual &&
            (!status.foreground || event.foregroundGeneration != status.foregroundGeneration))
        {
            continue;
        }
        cmd_registry.add<CommandTrackInput>(event);
    }
    if (!status.preview && status.foreground && !status.paused && status.available &&
        status.appProfile == Platform::AppProfile::Blender &&
        (status.blenderObservation.connected || previous.blenderObservation.connected))
    {
        cmd_registry.add<CommandObserveBlender>(status.blenderObservation);
    }
    Global::Input::status = status;
    Global::Input::pointer = pointer;
    Global::quit = status.quit;
    previous = status;
    initial = false;
    return changed || pointer_changed;
}
