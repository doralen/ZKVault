#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "app/frontend_contract.hpp"
#include "app/vault_app.hpp"
#include "crypto/secure_memory.hpp"
#include "model/password_entry.hpp"
#include "shell/interactive_shell.hpp"
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

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            PrintFrontendResult(BuildCliUsageResult());
            return 1;
        }

        const std::string command = argv[1];

        if (command == "shell") {
            if (argc != 2) {
                PrintFrontendResult(BuildCliUsageResult());
                return 1;
            }

            return RunInteractiveShell();
        }

        if (command == "init") {
            if (argc != 2) {
                PrintFrontendResult(BuildCliUsageResult());
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
            PrintFrontendResult(BuildInitializedResult(result.master_key_path));
            return 0;
        }

        if (command == "change-master-password") {
            if (argc != 2) {
                PrintFrontendResult(BuildCliUsageResult());
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
            PrintFrontendResult(BuildUpdatedResult(result.master_key_path));
            return 0;
        }

        if (command == "add" || command == "update") {
            if (argc != 3) {
                PrintFrontendResult(BuildCliUsageResult());
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
                PrintFrontendResult(BuildStoredEntryResult(result.entry_path));
            } else {
                PrintFrontendResult(BuildUpdatedResult(result.entry_path));
            }
            return 0;
        }

        if (command == "get") {
            if (argc != 3) {
                PrintFrontendResult(BuildCliUsageResult());
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
            PrintFrontendResult(BuildShowEntryResult(std::move(entry)));
            return 0;
        }

        if (command == "delete") {
            if (argc != 3) {
                PrintFrontendResult(BuildCliUsageResult());
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
            PrintFrontendResult(BuildDeletedEntryResult(result.entry_path));
            return 0;
        }

        if (command == "list") {
            if (argc != 2) {
                PrintFrontendResult(BuildCliUsageResult());
                return 1;
            }

            PrintFrontendResult(BuildListResult(ListEntryNames(), ""));
            return 0;
        }

        PrintFrontendResult(BuildCliUsageResult());
        return 1;
    } catch (const std::exception& ex) {
        FrontendError error = ClassifyFrontendError(ex.what());
        std::string output = RenderFrontendError(error);
        auto error_guard = MakeScopedCleanse(error);
        auto output_guard = MakeScopedCleanse(output);
        std::cerr << output << '\n';
        return 1;
    }
}
