// gameLogic/blender_tracker.hpp
#pragma once
#include "gameLogic/blender_state.hpp"
#include "platform/input_event.hpp"

void TrackBlenderInput(BlenderState& state, const Platform::InputEvent& event);
void ObserveBlenderState(BlenderState& state, const Platform::BlenderObservation& observation);
void ResetBlenderState(BlenderState& state, BlenderMode mode, const char* reason);
void InvalidateBlenderState(BlenderState& state, const char* reason,
                            BlenderUncertainty uncertainty = BlenderUncertainty::Mode);
