#include "tui/terminal_ui_forms.hpp"

#include <exception>
#include <string>

#include "crypto/secure_memory.hpp"
#include "shell/shell_runtime.hpp"
#include "tui/terminal_ui_runtime.hpp"

namespace tui_internal {
namespace {

void CancelEntryForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    const std::string status_message =
        render_state.entry_form.mode == EntryMutationMode::kCreate
            ? "entry creation cancelled"
            : "entry update cancelled";
    ClearEntryForm(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, status_message);
}

void CancelMasterPasswordForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearMasterPasswordForm(render_state);
    RestoreBrowseView(runtime);
    ReplaceStatusMessage(render_state, "master password rotation cancelled");
}

void CancelBrowseFilterForm(TuiRenderState& render_state) {
    ClearBrowseFilterForm(render_state);
    ReplaceStatusMessage(render_state, "browse filter cancelled");
}

void SubmitEntryForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    try {
        FrontendActionResult result = StoreShellEntryWithContent(
            runtime,
            render_state.entry_form.mode,
            render_state.entry_form.name,
            render_state.entry_form.password,
            render_state.entry_form.note);
        auto result_guard = MakeScopedCleanse(result);
        ClearEntryForm(render_state);
        RestoreBrowseView(runtime);
        ApplyTuiResultStatus(render_state, result);
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

void SubmitMasterPasswordForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    if (render_state.master_password_form.new_master_password !=
        render_state.master_password_form.confirm_master_password) {
        ::Cleanse(render_state.master_password_form.confirm_master_password);
        render_state.master_password_form.confirm_master_password.clear();
        render_state.master_password_form.field =
            TuiMasterPasswordFormField::kConfirmPassword;
        ReplaceStatusWithError(
            render_state,
            "new master passwords do not match");
        return;
    }

    try {
        FrontendActionResult result = RotateShellMasterPassword(
            runtime,
            render_state.master_password_form.new_master_password);
        auto result_guard = MakeScopedCleanse(result);
        ClearMasterPasswordForm(render_state);
        RestoreBrowseView(runtime);
        ApplyTuiResultStatus(render_state, result);
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

void SubmitBrowseFilterForm(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    try {
        FrontendActionResult result = ExecuteShellCommand(
            runtime,
            FrontendCommand{
                FrontendCommandKind::kFind,
                render_state.browse_filter.term
            });
        auto result_guard = MakeScopedCleanse(result);
        ClearBrowseFilterForm(render_state);
        ApplyTuiResultStatus(render_state, result);
    } catch (const std::exception& ex) {
        ReplaceStatusWithError(render_state, ex.what());
    }
}

}  // namespace

void BeginAddEntryFlow(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearMasterPasswordForm(render_state);
    ClearBrowseFilterForm(render_state);
    ClearExactConfirmation(render_state);
    BeginEntryForm(render_state, EntryMutationMode::kCreate, "");
    ReplaceStatusMessage(render_state, "creating entry");
    static_cast<void>(runtime.state_machine.HandleCommand(FrontendCommandKind::kAdd));
}

void BeginBrowseFilterFlow(TuiRenderState& render_state) {
    ClearPendingCommand(render_state);
    ClearEntryForm(render_state);
    ClearMasterPasswordForm(render_state);
    ClearExactConfirmation(render_state);
    BeginBrowseFilterForm(render_state);
    ReplaceStatusMessage(
        render_state,
        "type a filter term; submit an empty value to clear the current filter");
}

bool HandleEntryFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.entry_form.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelEntryForm(runtime, render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(ActiveEntryFormFieldValue(render_state.entry_form));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveEntryFormFieldValue(render_state.entry_form).push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kNextField) {
        AdvanceEntryFormField(render_state.entry_form);
        return true;
    }

    if (input_event.key != TuiKey::kShowSelection) {
        return true;
    }

    if (render_state.entry_form.field != TuiEntryFormField::kNote) {
        AdvanceEntryFormField(render_state.entry_form);
        return true;
    }

    SubmitEntryForm(runtime, render_state);
    return true;
}

bool HandleBrowseFilterInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.browse_filter.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelBrowseFilterForm(render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(
            ActiveBrowseFilterFieldValue(render_state.browse_filter));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveBrowseFilterFieldValue(render_state.browse_filter).push_back(
            input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kShowSelection) {
        SubmitBrowseFilterForm(runtime, render_state);
        return true;
    }

    return true;
}

bool HandleMasterPasswordFormInput(
    ShellRuntimeState& runtime,
    TuiRenderState& render_state,
    const TuiInputEvent& input_event) {
    if (!render_state.master_password_form.active) {
        return false;
    }

    if (input_event.key == TuiKey::kBrowse) {
        CancelMasterPasswordForm(runtime, render_state);
        return true;
    }

    if (input_event.key == TuiKey::kBackspace) {
        EraseLastCharacter(
            ActiveMasterPasswordFormFieldValue(
                render_state.master_password_form));
        return true;
    }

    if (input_event.text != '\0') {
        ActiveMasterPasswordFormFieldValue(
            render_state.master_password_form).push_back(input_event.text);
        return true;
    }

    if (input_event.key == TuiKey::kNextField) {
        AdvanceMasterPasswordFormField(render_state.master_password_form);
        return true;
    }

    if (input_event.key != TuiKey::kShowSelection) {
        return true;
    }

    if (render_state.master_password_form.field !=
        TuiMasterPasswordFormField::kConfirmPassword) {
        AdvanceMasterPasswordFormField(render_state.master_password_form);
        return true;
    }

    SubmitMasterPasswordForm(runtime, render_state);
    return true;
}

}  // namespace tui_internal
