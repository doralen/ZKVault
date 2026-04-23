#pragma once

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

void BeginUpdateEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state);

void BeginDeleteEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state);

void BeginMasterPasswordRotationFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state);

bool HandleExactConfirmationInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event);

}  // namespace tui_internal
