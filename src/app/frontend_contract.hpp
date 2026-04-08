#pragma once

#include <string>
#include <vector>

enum class FrontendCommandKind {
    kHelp,
    kList,
    kShow,
    kAdd,
    kUpdate,
    kDelete,
    kChangeMasterPassword,
    kQuit
};

struct FrontendCommand {
    FrontendCommandKind kind;
    std::string name;
};

struct ExactConfirmationRule {
    std::string prompt;
    std::string expected_value;
    std::string mismatch_error;
};

const std::vector<std::string>& CliUsageCommands();

const std::vector<std::string>& ShellHelpCommands();

FrontendCommand ParseShellCommand(const std::string& line);

ExactConfirmationRule BuildOverwriteConfirmationRule(const std::string& name);

ExactConfirmationRule BuildDeletionConfirmationRule(const std::string& name);

ExactConfirmationRule BuildMasterPasswordRotationConfirmationRule();

std::string FormatStoredEntryMessage(const std::string& entry_path);

std::string FormatUpdatedPathMessage(const std::string& path);

std::string FormatDeletedEntryMessage(const std::string& entry_path);
