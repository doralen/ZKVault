#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "app/frontend_contract.hpp"

namespace {

void Require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void RequireThrows(
    const std::function<void()>& fn,
    std::string_view expected_message) {
    try {
        fn();
    } catch (const std::exception& ex) {
        Require(ex.what() == expected_message,
                "exception message should match expected frontend contract");
        return;
    }

    throw std::runtime_error("expected function to throw");
}

void TestCliUsageCommands() {
    const auto& commands = CliUsageCommands();
    Require(commands.size() == 8, "cli usage should expose 8 commands");
    Require(commands[0] == "zkvault init", "cli usage should include init");
    Require(commands[1] == "zkvault shell", "cli usage should include shell");
    Require(commands[2] == "zkvault change-master-password",
            "cli usage should include master password rotation");
    Require(commands[7] == "zkvault list", "cli usage should include list");
}

void TestShellHelpCommands() {
    const auto& commands = ShellHelpCommands();
    Require(commands.size() == 8, "shell help should expose 8 commands");
    Require(commands[0] == "help", "shell help should include help");
    Require(commands[2] == "show <name>", "shell help should include show");
    Require(commands[6] == "change-master-password",
            "shell help should include rotation");
    Require(commands[7] == "quit", "shell help should include quit");
}

void TestShellCommandParsing() {
    const FrontendCommand show = ParseShellCommand("show email");
    Require(show.kind == FrontendCommandKind::kShow,
            "show command should parse as show");
    Require(show.name == "email", "show command should parse entry name");

    const FrontendCommand add = ParseShellCommand(" add   bank ");
    Require(add.kind == FrontendCommandKind::kAdd,
            "add command should parse as add");
    Require(add.name == "bank", "add command should ignore extra spacing");

    const FrontendCommand quit = ParseShellCommand("exit");
    Require(quit.kind == FrontendCommandKind::kQuit,
            "exit should map to quit");

    RequireThrows(
        [] { static_cast<void>(ParseShellCommand("update")); },
        "usage: update <name>");
    RequireThrows(
        [] { static_cast<void>(ParseShellCommand("list extra")); },
        "usage: list");
    RequireThrows(
        [] { static_cast<void>(ParseShellCommand("unknown")); },
        "unknown shell command");
}

void TestConfirmationRules() {
    const ExactConfirmationRule overwrite =
        BuildOverwriteConfirmationRule("email");
    Require(overwrite.prompt == "Type the entry name to confirm overwrite: ",
            "overwrite prompt should match");
    Require(overwrite.expected_value == "email",
            "overwrite expected value should match");
    Require(overwrite.mismatch_error == "entry overwrite cancelled",
            "overwrite mismatch error should match");

    const ExactConfirmationRule deletion =
        BuildDeletionConfirmationRule("bank");
    Require(deletion.prompt == "Type the entry name to confirm deletion: ",
            "deletion prompt should match");
    Require(deletion.expected_value == "bank",
            "deletion expected value should match");
    Require(deletion.mismatch_error == "entry deletion cancelled",
            "deletion mismatch error should match");

    const ExactConfirmationRule rotation =
        BuildMasterPasswordRotationConfirmationRule();
    Require(rotation.prompt ==
                "Type CHANGE to confirm master password rotation: ",
            "rotation prompt should match");
    Require(rotation.expected_value == "CHANGE",
            "rotation expected value should match");
    Require(rotation.mismatch_error == "master password rotation cancelled",
            "rotation mismatch error should match");
}

void TestOutputFormatting() {
    Require(FormatStoredEntryMessage("data/email.zkv") ==
                "saved to data/email.zkv",
            "store output should match");
    Require(FormatUpdatedPathMessage(".zkv_master") == "updated .zkv_master",
            "updated output should match");
    Require(FormatDeletedEntryMessage("data/email.zkv") ==
                "deleted data/email.zkv",
            "delete output should match");
}

}  // namespace

int main() {
    try {
        TestCliUsageCommands();
        TestShellHelpCommands();
        TestShellCommandParsing();
        TestConfirmationRules();
        TestOutputFormatting();
        return 0;
    } catch (const std::exception& ex) {
        return (std::fprintf(stderr, "frontend contract test failed: %s\n", ex.what()), 1);
    }
}
