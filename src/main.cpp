#include <iostream>
#include <stdexcept>
#include <string>

#include "app/frontend_contract.hpp"
#include "app/vault_app.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "shell/interactive_shell.hpp"
#include "terminal/prompt.hpp"

namespace {

void PrintUsage() {
    std::cout << "Usage:\n";
    for (const std::string& command : CliUsageCommands()) {
        std::cout << "  " << command << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            PrintUsage();
            return 1;
        }

        const std::string command = argv[1];

        if (command == "shell") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            return RunInteractiveShell();
        }

        if (command == "init") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            InitializeVaultRequest request{
                ReadConfirmedSecret(
                    "Master password: ",
                    "Confirm master password: ",
                    "master passwords do not match")
            };
            auto request_guard = MakeScopedCleanse(request);
            const InitializeVaultResult result = InitializeVault(request);
            std::cout << "initialized " << result.master_key_path << '\n';
            return 0;
        }

        if (command == "change-master-password") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            const ExactConfirmationRule rule =
                BuildMasterPasswordRotationConfirmationRule();
            RequireExactConfirmation(
                rule.prompt,
                rule.expected_value,
                rule.mismatch_error);
            RotateMasterPasswordRequest request{
                ReadSecret("Current master password: "),
                ReadConfirmedSecret(
                    "New master password: ",
                    "Confirm new master password: ",
                    "new master passwords do not match")
            };
            auto request_guard = MakeScopedCleanse(request);
            const RotateMasterPasswordResult result = RotateMasterPassword(request);
            std::cout << FormatUpdatedPathMessage(result.master_key_path) << '\n';
            return 0;
        }

        if (command == "add" || command == "update") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            const bool entry_exists = EntryExists(argv[2]);
            if (command == "add" && entry_exists) {
                throw std::runtime_error("entry already exists");
            }

            if (command == "update" && !entry_exists) {
                throw std::runtime_error("entry does not exist");
            }

            if (command == "update") {
                const ExactConfirmationRule rule =
                    BuildOverwriteConfirmationRule(argv[2]);
                RequireExactConfirmation(
                    rule.prompt,
                    rule.expected_value,
                    rule.mismatch_error);
            }

            StorePasswordEntryRequest request{
                command == "add" ? EntryMutationMode::kCreate
                                 : EntryMutationMode::kUpdate,
                argv[2],
                ReadSecret("Master password: "),
                ReadSecret("Entry password: "),
                ReadLine("Note: ")
            };
            auto request_guard = MakeScopedCleanse(request);
            const StorePasswordEntryResult result = StorePasswordEntry(request);
            if (command == "add") {
                std::cout << FormatStoredEntryMessage(result.entry_path) << '\n';
            } else {
                std::cout << FormatUpdatedPathMessage(result.entry_path) << '\n';
            }
            return 0;
        }

        if (command == "get") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            if (!EntryExists(argv[2])) {
                throw std::runtime_error("entry does not exist");
            }

            LoadPasswordEntryRequest request{
                argv[2],
                ReadSecret("Master password: ")
            };
            auto request_guard = MakeScopedCleanse(request);
            PasswordEntry entry = LoadPasswordEntry(request);
            auto entry_guard = MakeScopedCleanse(entry);
            std::string output = json(entry).dump(2);
            auto output_guard = MakeScopedCleanse(output);
            std::cout << output << '\n';
            return 0;
        }

        if (command == "delete") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            if (!EntryExists(argv[2])) {
                throw std::runtime_error("entry does not exist");
            }

            const ExactConfirmationRule rule =
                BuildDeletionConfirmationRule(argv[2]);
            RequireExactConfirmation(
                rule.prompt,
                rule.expected_value,
                rule.mismatch_error);
            const RemovePasswordEntryResult result =
                RemovePasswordEntry(RemovePasswordEntryRequest{argv[2]});
            std::cout << FormatDeletedEntryMessage(result.entry_path) << '\n';
            return 0;
        }

        if (command == "list") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            for (const std::string& name : ListEntryNames()) {
                std::cout << name << '\n';
            }
            return 0;
        }

        PrintUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
