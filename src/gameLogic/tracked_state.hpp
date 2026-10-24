// gameLogic/tracked_state.hpp
#pragma once
#include "gameLogic/blender_state.hpp"
#include "gameLogic/vim_state.hpp"

struct TrackedState
{
    Platform::AppProfile appProfile = Platform::AppProfile::Zed;
    VimState vim{};
    BlenderState blender{};
};

inline Confidence TrackedConfidence(const TrackedState& state)
{
    return state.appProfile == Platform::AppProfile::Blender ? state.blender.confidence
                                                             : state.vim.confidence;
}
