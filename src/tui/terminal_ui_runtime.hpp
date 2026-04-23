#pragma once

#include <string_view>

#include "tui/terminal_ui_state.hpp"

namespace tui_internal {

void ActivateBrowseView(ShellRuntimeState& runtime);

void ReplaceStatusWithError(
    TuiRenderState& render_state,
    std::string_view message);

void RestoreBrowseView(ShellRuntimeState& runtime);

void ClearTransientUiState(TuiRenderState& render_state);

void ApplyTuiResultStatus(
    TuiRenderState& render_state,
    const FrontendActionResult& result);

void HandleTuiFailure(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    std::string_view message);

}  // namespace tui_internal
