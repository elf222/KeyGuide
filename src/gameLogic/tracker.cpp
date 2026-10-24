// gameLogic/tracker.cpp
#include "gameLogic/tracker.hpp"
#include "gameLogic/blender_tracker.hpp"
#include "gameLogic/vim_tracker.hpp"

void TrackInput(TrackedState& state, const Platform::InputEvent& event)
{
    if (state.appProfile == Platform::AppProfile::Blender)
    {
        TrackBlenderInput(state.blender, event);
    }
    else if (event.type != Platform::EventType::ObjectBaseline &&
             event.type != Platform::EventType::MeshEditBaseline &&
             event.type != Platform::EventType::CurveEditBaseline &&
             event.type != Platform::EventType::NodeEditorBaseline &&
             !(event.type == Platform::EventType::MouseAction && !event.mousePressed))
    {
        TrackInput(state.vim, event);
    }
}

void TrackFocusChange(TrackedState& state, bool foreground)
{
    if (state.appProfile != Platform::AppProfile::Blender)
    {
        return;
    }
    if (state.blender.operation != BlenderOperation::None ||
        state.blender.context != BlenderContext::Editor)
    {
        const auto& blender = state.blender;
        const bool base_known = blender.context == BlenderContext::Editor ||
                                blender.context == BlenderContext::Rename ||
                                blender.context == BlenderContext::DeleteConfirm ||
                                (blender.context == BlenderContext::Menu &&
                                 blender.menu == BlenderMenuKind::PreserveMode);
        InvalidateBlenderState(
            state.blender,
            "Focus changed during a tool / popup; check Blender context before continuing",
            base_known ? BlenderUncertainty::Tool : BlenderUncertainty::Mode);
    }
    else if (foreground && state.blender.confidence != Confidence::Uncertain)
    {
        state.blender.confidence = Confidence::Assumed;
        state.blender.reason = "Previous Blender mode / editor assumed on return";
    }
}

void SetAppProfile(TrackedState& state, Platform::AppProfile profile)
{
    if (state.appProfile == profile)
    {
        return;
    }
    TrackFocusChange(state, false);
    state.appProfile = profile;
    if (profile != Platform::AppProfile::Blender)
    {
        state.vim.appProfile = profile;
        state.vim.recording = false;
        ResetTrackedState(state.vim, VimMode::Normal,
                          "Normal assumed after application profile change");
    }
}

void InvalidateTrackedState(TrackedState& state, const char* reason)
{
    if (state.appProfile == Platform::AppProfile::Blender)
    {
        InvalidateBlenderState(state.blender, reason);
    }
    else
    {
        InvalidateTrackedState(state.vim, reason);
    }
}
