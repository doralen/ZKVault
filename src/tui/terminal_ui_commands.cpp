#include "tui/terminal_ui_commands.hpp"

#include <exception>
#include <optional>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "tui/terminal_ui_confirmations.hpp"
#include "tui/terminal_ui_forms.hpp"
#include "tui/terminal_ui_render.hpp"
#include "tui/terminal_ui_runtime.hpp"

namespace tui_internal {

std::optional<FrontendCommand> ResolveTuiCommand(
    const ShellRuntimeState& runtime,
    const TuiInputEvent& input_event) {
    switch (input_event.key) {
        case TuiKey::kMoveUp:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kPrev, ""};
        case TuiKey::kMoveDown:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kNext, ""};
        case TuiKey::kShowSelection:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kShow, ""};
        case TuiKey::kBrowse:
            if (!runtime.session.has_value()) {
                return std::nullopt;
            }
            return FrontendCommand{FrontendCommandKind::kList, ""};
        case TuiKey::kCharacter: {
            const char ch = input_event.text;
            if (ch == '?') {
                return FrontendCommand{FrontendCommandKind::kHelp, ""};
            }

            const unsigned char value = static_cast<unsigned char>(ch);
            const char lower =
                value >= 'A' && value <= 'Z'
                    ? static_cast<char>(value - 'A' + 'a')
                    : static_cast<char>(value);
            switch (lower) {
                case 'j':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kNext, ""};
                case 'k':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kPrev, ""};
                case 'a':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kAdd, ""};
                case 'f':
                case '/':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kFind, ""};
                case 'e':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kUpdate, ""};
                case 'd':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{FrontendCommandKind::kDelete, ""};
                case 'm':
                    if (!runtime.session.has_value()) {
                        return std::nullopt;
                    }
                    return FrontendCommand{
                        FrontendCommandKind::kChangeMasterPassword,
                        ""
                    };
                case 'l':
                    return FrontendCommand{FrontendCommandKind::kLock, ""};
                case 'u':
                    return FrontendCommand{FrontendCommandKind::kUnlock, ""};
                case 'q':
                    return FrontendCommand{FrontendCommandKind::kQuit, ""};
                default:
                    return std::nullopt;
            }
        }
        default:
            return std::nullopt;
    }
}

bool HandleResolvedTuiCommand(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const FrontendCommand& command) {
    try {
        if (command.kind == FrontendCommandKind::kAdd) {
            BeginAddEntryFlow(runtime, render_state);
            return false;
        }

        if (command.kind == FrontendCommandKind::kUpdate) {
            BeginUpdateEntryFlow(runtime, render_state);
            return false;
        }

        if (command.kind == FrontendCommandKind::kDelete) {
            BeginDeleteEntryFlow(runtime, render_state);
            return false;
        }

        if (command.kind == FrontendCommandKind::kFind) {
            BeginBrowseFilterFlow(render_state);
            return false;
        }

        if (command.kind == FrontendCommandKind::kChangeMasterPassword) {
            BeginMasterPasswordRotationFlow(runtime, render_state);
            return false;
        }

        FrontendActionResult result{};
        if (command.kind == FrontendCommandKind::kList) {
            result = ShowCurrentShellBrowseView(runtime);
        } else if (ShouldPreviewPreparedCommand(runtime, command)) {
            ReplacePendingCommand(render_state, command);
            ReplaceStatusMessage(
                render_state,
                BuildPendingStatusMessage(command));
            static_cast<void>(runtime.state_machine.HandleCommand(command.kind));
            RenderScreen(runtime, render_state);
            result = ExecutePreparedShellCommand(runtime, command);
            if (command.kind == FrontendCommandKind::kUnlock &&
                runtime.session.has_value()) {
                ActivateBrowseView(runtime);
            }
            ClearPendingCommand(render_state);
        } else {
            result = ExecuteShellCommand(runtime, command);
        }
        auto result_guard = MakeScopedCleanse(result);
        ApplyTuiResultStatus(render_state, result);
        return runtime.state_machine.state() ==
               FrontendSessionState::kQuitRequested;
    } catch (const std::exception& ex) {
        HandleTuiFailure(runtime, render_state, ex.what());
        return false;
    }
}

}  // namespace tui_internal
