#include "shell/interactive_shell.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/frontend_contract.hpp"
#include "app/vault_app.hpp"
#include "app/vault_session.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "terminal/prompt.hpp"

namespace {

void PrintEntry(const PasswordEntry& entry) {
    std::string output = json(entry).dump(2);
    auto output_guard = MakeScopedCleanse(output);
    std::cout << output << '\n';
}

void PrintShellHelp() {
    std::cout << "Commands:\n";
    for (const std::string& command : ShellHelpCommands()) {
        std::cout << "  " << command << '\n';
    }
}

VaultSession OpenOrInitializeSession() {
    if (!std::filesystem::exists(".zkv_master")) {
        const std::string choice = ReadLine(
            "Vault not initialized. Create one now? [y/N]: ");
        if (choice != "y" && choice != "Y" && choice != "yes" && choice != "YES") {
            throw std::runtime_error("vault not initialized");
        }

        InitializeVaultRequest init_request{
            ReadConfirmedSecret(
                "Master password: ",
                "Confirm master password: ",
                "master passwords do not match")
        };
        auto init_request_guard = MakeScopedCleanse(init_request);
        const InitializeVaultResult result = InitializeVault(init_request);
        std::cout << "initialized " << result.master_key_path << '\n';
        return VaultSession::Open(init_request.master_password);
    }

    std::string master_password = ReadSecret("Master password: ");
    auto master_password_guard = MakeScopedCleanse(master_password);
    return VaultSession::Open(master_password);
}

void PrintList(VaultSession& session) {
    const std::vector<std::string> names = session.ListEntryNames();
    if (names.empty()) {
        std::cout << "(empty)\n";
        return;
    }

    for (const std::string& name : names) {
        std::cout << name << '\n';
    }
}

void ExecuteShellCommand(
    VaultSession& session,
    const FrontendCommand& command,
    bool& should_quit) {
    if (command.kind == FrontendCommandKind::kHelp) {
        PrintShellHelp();
        return;
    }

    if (command.kind == FrontendCommandKind::kList) {
        PrintList(session);
        return;
    }

    if (command.kind == FrontendCommandKind::kShow) {
        PasswordEntry entry = session.LoadEntry(command.name);
        auto entry_guard = MakeScopedCleanse(entry);
        PrintEntry(entry);
        return;
    }

    if (command.kind == FrontendCommandKind::kAdd) {
        StorePasswordEntryRequest request{
            EntryMutationMode::kCreate,
            command.name,
            "",
            ReadSecret("Entry password: "),
            ReadLine("Note: ")
        };
        auto request_guard = MakeScopedCleanse(request);
        const StorePasswordEntryResult result = session.StoreEntry(request);
        std::cout << FormatStoredEntryMessage(result.entry_path) << '\n';
        return;
    }

    if (command.kind == FrontendCommandKind::kUpdate) {
        const ExactConfirmationRule rule =
            BuildOverwriteConfirmationRule(command.name);
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        StorePasswordEntryRequest request{
            EntryMutationMode::kUpdate,
            command.name,
            "",
            ReadSecret("Entry password: "),
            ReadLine("Note: ")
        };
        auto request_guard = MakeScopedCleanse(request);
        const StorePasswordEntryResult result = session.StoreEntry(request);
        std::cout << FormatUpdatedPathMessage(result.entry_path) << '\n';
        return;
    }

    if (command.kind == FrontendCommandKind::kDelete) {
        const ExactConfirmationRule rule =
            BuildDeletionConfirmationRule(command.name);
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        const RemovePasswordEntryResult result = session.RemoveEntry(command.name);
        std::cout << FormatDeletedEntryMessage(result.entry_path) << '\n';
        return;
    }

    if (command.kind == FrontendCommandKind::kChangeMasterPassword) {
        const ExactConfirmationRule rule =
            BuildMasterPasswordRotationConfirmationRule();
        RequireExactConfirmation(
            rule.prompt,
            rule.expected_value,
            rule.mismatch_error);
        std::string new_master_password = ReadConfirmedSecret(
            "New master password: ",
            "Confirm new master password: ",
            "new master passwords do not match");
        auto new_master_password_guard = MakeScopedCleanse(new_master_password);
        const RotateMasterPasswordResult result =
            session.RotateMasterPassword(new_master_password);
        std::cout << FormatUpdatedPathMessage(result.master_key_path) << '\n';
        return;
    }

    if (command.kind == FrontendCommandKind::kQuit) {
        should_quit = true;
        return;
    }
}

}  // namespace

int RunInteractiveShell() {
    VaultSession session = OpenOrInitializeSession();

    std::cout << "shell ready; type help for commands\n";

    std::string line;
    while (true) {
        if (!TryReadLine("zkvault> ", line)) {
            std::cout << '\n';
            return 0;
        }

        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        try {
            bool should_quit = false;
            const FrontendCommand command = ParseShellCommand(line);
            ExecuteShellCommand(session, command, should_quit);
            if (should_quit) {
                return 0;
            }
        } catch (const std::exception& ex) {
            std::cout << "error: " << ex.what() << '\n';
        }
    }
}
