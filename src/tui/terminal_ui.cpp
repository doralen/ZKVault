#include "tui/terminal_ui.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <unistd.h>

#include "app/frontend_contract.hpp"
#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "terminal/display.hpp"
#include "terminal/prompt.hpp"

namespace {

constexpr std::string_view kTuiPrompt = "zkvault[tui]> ";

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

void ReplaceRenderedOutput(
    std::string& rendered_output,
    std::string next_output) {
    Cleanse(rendered_output);
    rendered_output = std::move(next_output);
}

std::string RenderTuiActionOutput(const FrontendActionResult& result) {
    if (result.payload_kind == FrontendPayloadKind::kEntryNames ||
        result.payload_kind == FrontendPayloadKind::kFocusedList ||
        result.payload_kind == FrontendPayloadKind::kNone) {
        return "";
    }

    return RenderFrontendActionResult(result);
}

PromptReadStatus ReadTuiCommandLine(
    const std::optional<std::chrono::milliseconds>& idle_timeout,
    bool session_unlocked,
    std::string& line) {
    if (session_unlocked && idle_timeout.has_value()) {
        return TryReadLineWithTimeout(
            std::string(kTuiPrompt),
            line,
            *idle_timeout);
    }

    return TryReadLine(std::string(kTuiPrompt), line)
               ? PromptReadStatus::kRead
               : PromptReadStatus::kEof;
}

void RenderBrowseSnapshot(const ShellBrowseSnapshot& snapshot) {
    std::cout << "Entries";
    if (!snapshot.filter_term.empty()) {
        std::cout << " [filter: " << snapshot.filter_term << "]";
    }
    std::cout << ":\n";

    if (snapshot.entry_names.empty()) {
        std::cout << snapshot.empty_message << '\n';
        return;
    }

    for (const std::string& entry_name : snapshot.entry_names) {
        const bool is_selected =
            !snapshot.selected_name.empty() &&
            entry_name == snapshot.selected_name;
        std::cout << (is_selected ? "> " : "  ") << entry_name << '\n';
    }
}

void RenderScreen(
    const ShellRuntimeState& runtime,
    std::string_view last_output) {
    if (ShouldEmitTerminalControlSequences(STDOUT_FILENO)) {
        std::cout << BuildClearScreenSequence();
    }

    std::cout << "ZKVault TUI Prototype\n";
    std::cout << "Session: "
              << (ShellSessionUnlocked(runtime) ? "unlocked" : "locked")
              << "\n\n";

    if (!last_output.empty()) {
        std::cout << "Output:\n" << last_output << "\n\n";
    }

    ShellBrowseSnapshot snapshot = SnapshotShellBrowseState(runtime);
    auto snapshot_guard = MakeScopedCleanse(snapshot);
    RenderBrowseSnapshot(snapshot);

    std::cout << "\nType `help` for available commands.\n";
    std::cout.flush();
}

}  // namespace

int RunTerminalUi() {
    ScopedAlternateScreen alternate_screen;

    OpenShellRuntimeResult open_result = OpenOrInitializeShellRuntime();
    auto open_result_guard = MakeScopedCleanse(open_result);
    ShellRuntimeState& runtime = open_result.runtime;
    const std::optional<std::chrono::milliseconds> idle_timeout =
        ReadShellIdleTimeout();

    FrontendActionResult ready_result = BuildTuiReadyResult();
    auto ready_result_guard = MakeScopedCleanse(ready_result);
    static_cast<void>(runtime.state_machine.ApplyActionResult(ready_result));

    std::string last_output;
    auto last_output_guard = MakeScopedCleanse(last_output);

    std::string initial_output;
    if (open_result.startup_result.has_value()) {
        initial_output = RenderTuiActionOutput(*open_result.startup_result);
    }

    const std::string ready_output = RenderTuiActionOutput(ready_result);
    if (!initial_output.empty() && !ready_output.empty()) {
        initial_output += '\n';
    }
    initial_output += ready_output;
    ReplaceRenderedOutput(last_output, std::move(initial_output));

    std::string line;
    while (true) {
        RenderScreen(runtime, last_output);

        const PromptReadStatus read_status =
            ReadTuiCommandLine(
                idle_timeout,
                ShellSessionUnlocked(runtime),
                line);
        if (read_status == PromptReadStatus::kTimedOut) {
            Cleanse(line);
            line.clear();

            FrontendActionResult result = HandleShellIdleTimeout(runtime);
            auto result_guard = MakeScopedCleanse(result);
            ReplaceRenderedOutput(
                last_output,
                RenderTuiActionOutput(result));
            continue;
        }

        if (read_status == PromptReadStatus::kEof) {
            std::cout << '\n';
            return 0;
        }

        if (IsBlankShellInput(line)) {
            Cleanse(line);
            line.clear();
            continue;
        }

        try {
            const FrontendCommand command = ParseShellCommand(line);
            FrontendActionResult result = ExecuteShellCommand(runtime, command);
            auto result_guard = MakeScopedCleanse(result);
            ReplaceRenderedOutput(
                last_output,
                RenderTuiActionOutput(result));
            Cleanse(line);
            line.clear();
            if (runtime.state_machine.state() ==
                FrontendSessionState::kQuitRequested) {
                return 0;
            }
        } catch (const std::exception& ex) {
            FrontendError error = ClassifyFrontendError(ex.what());
            std::string output = RenderFrontendError(error);
            auto error_guard = MakeScopedCleanse(error);
            auto output_guard = MakeScopedCleanse(output);
            std::optional<FrontendActionResult> recovered_result =
                RecoverShellViewAfterFailure(runtime);
            if (recovered_result.has_value()) {
                std::string recovered_output =
                    RenderTuiActionOutput(*recovered_result);
                auto recovered_output_guard = MakeScopedCleanse(recovered_output);
                if (!recovered_output.empty()) {
                    output += '\n';
                    output += recovered_output;
                }
            }
            ReplaceRenderedOutput(last_output, std::move(output));
            Cleanse(line);
            line.clear();
        }
    }
}
