#pragma once

#include <optional>

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

std::optional<FrontendCommand> ResolveTuiCommand(
    const ShellRuntimeState& runtime,
    const TuiInputEvent& input_event);

bool HandleResolvedTuiCommand(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const FrontendCommand& command);

}  // namespace tui_internal
