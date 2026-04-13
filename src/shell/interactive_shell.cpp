#include "shell/interactive_shell.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>

#include "app/frontend_contract.hpp"
#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "terminal/display.hpp"
#include "terminal/prompt.hpp"

namespace {

void PrintFrontendResult(FrontendActionResult result) {
    auto result_guard = MakeScopedCleanse(result);
    std::string output = RenderFrontendActionResult(result);
    auto output_guard = MakeScopedCleanse(output);
    if (!output.empty()) {
        std::cout << output << '\n';
    }
}

PromptReadStatus ReadShellCommandLine(
    const std::optional<std::chrono::milliseconds>& idle_timeout,
    bool session_unlocked,
    std::string& line) {
    if (session_unlocked && idle_timeout.has_value()) {
        return TryReadLineWithTimeout("zkvault> ", line, *idle_timeout);
    }

    return TryReadLine("zkvault> ", line) ? PromptReadStatus::kRead
                                          : PromptReadStatus::kEof;
}

}  // namespace

int RunInteractiveShell() {
    OpenShellRuntimeResult open_result = OpenOrInitializeShellRuntime();
    auto open_result_guard = MakeScopedCleanse(open_result);
    ShellRuntimeState& runtime = open_result.runtime;
    const std::optional<std::chrono::milliseconds> idle_timeout =
        ReadShellIdleTimeout();
    if (open_result.startup_result.has_value()) {
        PrintFrontendResult(std::move(*open_result.startup_result));
    }
    PrintFrontendResult(BuildShellReadyResult());

    std::string line;
    while (true) {
        const PromptReadStatus read_status =
            ReadShellCommandLine(
                idle_timeout,
                ShellSessionUnlocked(runtime),
                line);
        if (read_status == PromptReadStatus::kTimedOut) {
            Cleanse(line);
            line.clear();
            ClearTerminalScreenIfInteractive();
            PrintFrontendResult(HandleShellIdleTimeout(runtime));
            continue;
        }

        if (read_status == PromptReadStatus::kEof) {
            std::cout << '\n';
            return 0;
        }

        if (IsBlankShellInput(line)) {
            continue;
        }

        try {
            const FrontendCommand command = ParseShellCommand(line);
            FrontendActionResult result = ExecuteShellCommand(runtime, command);
            if (command.kind == FrontendCommandKind::kLock) {
                ClearTerminalScreenIfInteractive();
            }
            PrintFrontendResult(std::move(result));
            if (runtime.state_machine.state() ==
                FrontendSessionState::kQuitRequested) {
                return 0;
            }
        } catch (const std::exception& ex) {
            FrontendError error = ClassifyFrontendError(ex.what());
            std::string output = RenderFrontendError(error);
            auto error_guard = MakeScopedCleanse(error);
            auto output_guard = MakeScopedCleanse(output);
            std::cout << output << '\n';
            std::optional<FrontendActionResult> recovered_result =
                RecoverShellViewAfterFailure(runtime);
            if (recovered_result.has_value()) {
                PrintFrontendResult(std::move(*recovered_result));
            }
        }
    }
}
