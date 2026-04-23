#include "tui/terminal_ui_flow.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <unistd.h>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "terminal/display.hpp"
#include "tui/terminal_ui_commands.hpp"
#include "tui/terminal_ui_confirmations.hpp"
#include "tui/terminal_ui_forms.hpp"
#include "tui/terminal_ui_input.hpp"
#include "tui/terminal_ui_render.hpp"
#include "tui/terminal_ui_runtime.hpp"
#include "tui/terminal_ui_state.hpp"

namespace tui_internal {
namespace {

class ScopedAlternateScreen {
public:
    ScopedAlternateScreen() {
        active_ = ShouldEmitTerminalControlSequences(STDOUT_FILENO);
        if (!active_) {
            return;
        }

        std::cout << BuildEnterAlternateScreenSequence()
                  << BuildClearScreenSequence();
        std::cout.flush();
    }

    ScopedAlternateScreen(const ScopedAlternateScreen&) = delete;
    ScopedAlternateScreen& operator=(const ScopedAlternateScreen&) = delete;

    ~ScopedAlternateScreen() {
        if (!active_) {
            return;
        }

        std::cout << BuildExitAlternateScreenSequence();
        std::cout.flush();
    }

private:
    bool active_ = false;
};

std::string BuildInitialStatusMessage(
    const OpenShellRuntimeResult& open_result,
    const FrontendActionResult& ready_result) {
    std::string initial_status;
    if (open_result.startup_result.has_value()) {
        initial_status = RenderTuiStatusMessage(*open_result.startup_result);
    }

    const std::string ready_status = RenderTuiStatusMessage(ready_result);
    if (!initial_status.empty() && !ready_status.empty()) {
        initial_status += '\n';
    }
    initial_status += ready_status;
    return initial_status;
}

}  // namespace

int RunTerminalUiLoop() {
    ScopedAlternateScreen alternate_screen;

    OpenShellRuntimeResult open_result = OpenOrInitializeShellRuntime();
    auto open_result_guard = MakeScopedCleanse(open_result);
    ShellRuntimeState& runtime = open_result.runtime;
    const std::optional<std::chrono::milliseconds> idle_timeout =
        ReadShellIdleTimeout();

    FrontendActionResult ready_result = BuildTuiReadyResult();
    auto ready_result_guard = MakeScopedCleanse(ready_result);
    static_cast<void>(runtime.state_machine.ApplyActionResult(ready_result));
    ActivateBrowseView(runtime);

    TuiRenderState render_state;
    auto render_state_guard = MakeScopedCleanse(render_state);
    ReplaceStatusMessage(
        render_state,
        BuildInitialStatusMessage(open_result, ready_result));

    while (true) {
        RenderScreen(runtime, render_state);

        const TuiInputEvent input_event = ReadTuiInput(
            ShellSessionUnlocked(runtime) ? idle_timeout : std::nullopt);
        if (input_event.status == TuiInputStatus::kTimedOut) {
            ClearTransientUiState(render_state);
            FrontendActionResult result = HandleShellIdleTimeout(runtime);
            auto result_guard = MakeScopedCleanse(result);
            ApplyTuiResultStatus(render_state, result);
            continue;
        }

        if (input_event.status == TuiInputStatus::kEof) {
            std::cout << '\n';
            return 0;
        }

        if (HandleEntryFormInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleBrowseFilterInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleMasterPasswordFormInput(runtime, render_state, input_event)) {
            continue;
        }

        if (HandleExactConfirmationInput(runtime, render_state, input_event)) {
            continue;
        }

        const std::optional<FrontendCommand> command =
            ResolveTuiCommand(runtime, input_event);
        if (!command.has_value()) {
            continue;
        }

        if (HandleResolvedTuiCommand(runtime, render_state, *command)) {
            return 0;
        }
    }
}

}  // namespace tui_internal
