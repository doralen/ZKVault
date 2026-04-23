#include "tui/terminal_ui_runtime.hpp"

#include <string>
#include <utility>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"

namespace tui_internal {

void ActivateBrowseView(ShellRuntimeState& runtime) {
    if (!runtime.session.has_value()) {
        return;
    }

    FrontendActionResult result = ShowCurrentShellBrowseView(runtime);
    auto result_guard = MakeScopedCleanse(result);
}

void ReplaceStatusWithError(
    TuiRenderState& render_state,
    std::string_view message) {
    FrontendError error = ClassifyFrontendError(message);
    std::string output = RenderFrontendError(error);
    auto error_guard = MakeScopedCleanse(error);
    auto output_guard = MakeScopedCleanse(output);
    ReplaceStatusMessage(render_state, std::move(output));
}

void RestoreBrowseView(ShellRuntimeState& runtime) {
    FrontendActionResult result = ShowCurrentShellBrowseView(runtime);
    auto result_guard = MakeScopedCleanse(result);
}

void ClearTransientUiState(TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    ClearExactConfirmation(render_state);
}

void ApplyTuiResultStatus(
    TuiRenderState& render_state,
    const FrontendActionResult& result) {
    const std::string status_message = RenderTuiStatusMessage(result);
    if (status_message.empty()) {
        ClearStatusMessage(render_state);
    } else {
        ReplaceStatusMessage(render_state, status_message);
    }
}

void HandleTuiFailure(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    std::string_view message) {
    FrontendError error = ClassifyFrontendError(message);
    std::string output = RenderFrontendError(error);
    auto error_guard = MakeScopedCleanse(error);
    auto output_guard = MakeScopedCleanse(output);
    ClearTransientUiState(render_state);
    static_cast<void>(RecoverShellViewAfterFailure(runtime));
    ReplaceStatusMessage(render_state, std::move(output));
}

}  // namespace tui_internal
