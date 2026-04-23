#include "tui/terminal_ui_confirmations.hpp"

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "tui/terminal_ui_runtime.hpp"

namespace tui_internal {
namespace {

void CancelExactConfirmation(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    std::string status_message) {
    ClearExactConfirmation(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, std::move(status_message));
}

void SubmitExactConfirmation(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    if (render_state.exact_confirmation.typed_value !=
        render_state.exact_confirmation.rule.expected_value) {
        CancelExactConfirmation(
            runtime,
            render_state,
            RenderFrontendError(FrontendError{
                FrontendErrorKind::kConfirmationRejected,
                render_state.exact_confirmation.rule.mismatch_error
            }));
        return;
    }

    try {
        if (render_state.exact_confirmation.kind ==
            FrontendCommandKind::kChangeMasterPassword) {
            static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
            ClearExactConfirmation(render_state);
            BeginMasterPasswordForm(render_state);
            ReplaceStatusMessage(
                render_state,
                "enter the new master password");
            return;
        }

        if (render_state.exact_confirmation.kind == FrontendCommandKind::kUpdate) {
            if (!runtime.session.has_value()) {
                throw std::runtime_error("vault is locked");
            }

            PasswordEntry entry =
                runtime.session->LoadEntry(render_state.exact_confirmation.entry_name);
            auto entry_guard = MakeScopedCleanse(entry);
            static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
            ClearExactConfirmation(render_state);
            PopulateEntryFormForUpdate(render_state, entry);
            ReplaceStatusMessage(
                render_state,
                "editing entry " + render_state.entry_form.name);
            return;
        }

        static_cast<void>(runtime.state_machine.HandleConfirmationAccepted());
        FrontendActionResult result = RemoveShellEntryByName(
            runtime,
            render_state.exact_confirmation.entry_name);
        auto result_guard = MakeScopedCleanse(result);
        ClearExactConfirmation(render_state);
        RestoreBrowseView(runtime);
        ApplyTuiResultStatus(render_state, result);
    } catch (const std::exception& ex) {
        ClearExactConfirmation(render_state);
        RestoreBrowseView(runtime);
        ReplaceStatusWithError(render_state, ex.what());
    }
}

std::string BuildExactConfirmationCancelMessage(
    const TuiExactConfirmationState& state) {
    if (state.kind == FrontendCommandKind::kUpdate) {
        return "entry update cancelled";
    }

    if (state.kind == FrontendCommandKind::kChangeMasterPassword) {
        return "master password rotation cancelled";
    }

    return "entry deletion cancelled";
}

}  // namespace

void BeginUpdateEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::optional<std::string> selected_name =
        SelectedBrowseEntryName(runtime);
    if (!selected_name.has_value()) {
        ReplaceStatusWithError(render_state, "no entry selected");
        return;
    }

    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kUpdate,
        *selected_name,
        BuildOverwriteConfirmationRule(*selected_name));
    ReplaceStatusMessage(
        render_state,
        "type the selected entry name to confirm update");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kUpdate));
}

void BeginDeleteEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::optional<std::string> selected_name =
        SelectedBrowseEntryName(runtime);
    if (!selected_name.has_value()) {
        ReplaceStatusWithError(render_state, "no entry selected");
        return;
    }

    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kDelete,
        *selected_name,
        BuildDeletionConfirmationRule(*selected_name));
    ReplaceStatusMessage(
        render_state,
        "type the selected entry name to confirm deletion");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kDelete));
}

void BeginMasterPasswordRotationFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    BeginExactConfirmation(
        render_state,
        FrontendCommandKind::kChangeMasterPassword,
        "",
        BuildMasterPasswordRotationConfirmationRule());
    ReplaceStatusMessage(
        render_state,
        "type CHANGE to confirm master password rotation");
    static_cast<void>(
        runtime.state_machine.HandleCommand(
            FrontendCommandKind::kChangeMasterPassword));
}

bool HandleExactConfirmationInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.exact_confirmation.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelExactConfirmation(
            runtime,
            render_state,
            BuildExactConfirmationCancelMessage(render_state.exact_confirmation));
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(render_state.exact_confirmation.typed_value);
        return true;
    }

    if (input_event.text != '\0') {
        render_state.exact_confirmation.typed_value.push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kShowSelection) {
        SubmitExactConfirmation(runtime, render_state);
        return true;
    }

    return true;
}

}  // namespace tui_internal
