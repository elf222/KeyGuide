// gameLogic/vim_tracker.hpp
#pragma once
#include "gameLogic/vim_state.hpp"
void TrackInput(VimState& state, const Platform::InputEvent& event);
void ResetTrackedState(VimState& state, VimMode mode, const char* reason);
void InvalidateTrackedState(VimState& state, const char* reason);
