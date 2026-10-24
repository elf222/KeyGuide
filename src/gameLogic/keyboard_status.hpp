// gameLogic/keyboard_status.hpp
#pragma once
#include <string>
#include <string_view>
#include "gameLogic/aerospace_bindings.hpp"
#include "gameLogic/tracked_state.hpp"

enum class StatusKind
{
    Mode,
    Inactive,
    Paused,
    Unavailable,
    Resync
};

struct KeyboardStatus
{
    StatusKind kind = StatusKind::Mode;
    std::string_view label;
    std::string pending, detail;
};

inline KeyboardStatus BuildKeyboardStatus(const VimState& state,
                                          const Platform::InputBatchInfo& input)
{
    KeyboardStatus status;
    if (input.paused)
    {
        status.kind = StatusKind::Paused;
        status.label = "PAUSED";
    }
    else if (!input.foreground)
    {
        status.kind = StatusKind::Inactive;
        status.label = "INACTIVE";
    }
    else if (!input.preview && !input.available)
    {
        status.kind = StatusKind::Unavailable;
        status.label = "INPUT OFF";
    }
    else if (state.confidence == Confidence::Uncertain)
    {
        status.kind = StatusKind::Resync;
        status.label = "RESYNC";
    }
    else
    {
        status.label = state.context == VimContext::CommandLine && state.mode != VimMode::Ex
                           ? "COMMAND"
                           : ModeName(state.mode);
        status.pending.assign(state.pendingSequence.data(), state.pendingLength);
        if (state.pendingTruncated)
        {
            status.pending += "..";
        }
        if (state.recording)
        {
            status.pending += status.pending.empty() ? "REC" : " · REC";
        }
    }
    status.detail = input.description;
    if (state.reason && *state.reason)
    {
        status.detail += '\n';
        status.detail += state.reason;
    }
    if (state.waiting != WaitingKind::None || state.operatorKey ||
        state.context != VimContext::Editor)
    {
        status.detail += '\n';
        status.detail += WaitingName(state);
    }
    return status;
}

inline KeyboardStatus BuildKeyboardStatus(const BlenderState& blender,
                                          const Platform::InputBatchInfo& input)
{
    KeyboardStatus status;
    if (input.paused)
    {
        status.kind = StatusKind::Paused;
        status.label = "PAUSED";
    }
    else if (!input.foreground)
    {
        status.kind = StatusKind::Inactive;
        status.label = "INACTIVE";
    }
    else if (!input.preview && !input.available)
    {
        status.kind = StatusKind::Unavailable;
        status.label = "INPUT OFF";
    }
    else if (blender.observation.connected && blender.confidence != Confidence::Uncertain &&
             !blender.observation.interfaceLocked &&
             (blender.editor == BlenderEditor::Unsupported ||
              (blender.editor == BlenderEditor::Viewport &&
               blender.mode == BlenderMode::Unsupported)))
    {
        status.label = std::string_view(blender.observation.editor.data()) == "VIEW_3D"
                           ? std::string_view(blender.observation.mode.data())
                           : std::string_view(blender.observation.editor.data());
    }
    else if (blender.confidence == Confidence::Uncertain)
    {
        status.kind = StatusKind::Resync;
        status.label = "RESYNC";
    }
    else
    {
        switch (blender.context)
        {
            case BlenderContext::Menu:
                status.label = "MENU";
                break;
            case BlenderContext::Search:
                status.label = "SEARCH";
                break;
            case BlenderContext::Rename:
                status.label = "RENAME";
                break;
            case BlenderContext::DeleteConfirm:
                status.label = "DELETE?";
                break;
            case BlenderContext::Editor:
                status.label = blender.operation == BlenderOperation::None
                                   ? BlenderBaseStateName(blender)
                                   : BlenderOperationName(blender.operation);
                break;
        }
        if (blender.axis)
        {
            status.pending = blender.constraint == BlenderConstraint::Plane ? "PLANE " : "";
            status.pending += blender.axis;
            if (blender.constraintCycle == 2)
            {
                status.pending += " LOCAL";
            }
        }
        if (blender.numericInput)
        {
            status.pending += status.pending.empty() ? "NUM" : " · NUM";
        }
    }
    status.detail = input.description;
    if (blender.observation.connected)
    {
        status.detail += blender.observation.modalAvailable
                             ? "\nBlender state bridge connected · mode and running tools observed"
                             : "\nBlender state bridge connected · mode observed; tools inferred";
        if (!blender.observation.current)
        {
            status.detail += "\nWaiting for a report after the latest input / pointer change";
        }
    }
    else
    {
        status.detail += "\nBlender state bridge not connected · passive inference fallback";
    }
    status.detail +=
        blender.uncertainty == BlenderUncertainty::Mode ? "\nLast assumed mode: " : "\nBase mode: ";
    status.detail += BlenderBaseStateName(blender);
    status.detail += "\n";
    status.detail += blender.reason;
    if (blender.confidence == Confidence::Uncertain)
    {
        status.detail +=
            blender.uncertainty == BlenderUncertainty::Tool
                ? "\nPress Escape in the active Blender editor to clear the interrupted tool."
                : "\nKeyGuide → Resync Mode: match the actual mode or Node Editor.";
    }
    return status;
}

inline KeyboardStatus BuildKeyboardStatus(const TrackedState& state,
                                          const Platform::InputBatchInfo& input,
                                          bool aerospace_service = false)
{
    auto status = state.appProfile == Platform::AppProfile::Blender
                      ? BuildKeyboardStatus(state.blender, input)
                      : BuildKeyboardStatus(state.vim, input);
    if (status.kind == StatusKind::Inactive && AeroSpaceHintsShown(input))
    {
        status.pending = aerospace_service ? "AEROSPACE · SERVICE" : "AEROSPACE";
        status.detail += aerospace_service ? "\nAeroSpace service mode shortcuts shown"
                                           : "\nHold Option for AeroSpace shortcuts";
    }
    return status;
}
