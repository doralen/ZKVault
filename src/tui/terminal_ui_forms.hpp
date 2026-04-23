#pragma once

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

void BeginAddEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state);

void BeginBrowseFilterFlow(TuiRenderState& render_state);

bool HandleEntryFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event);

bool HandleBrowseFilterInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event);

bool HandleMasterPasswordFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event);

}  // namespace tui_internal
