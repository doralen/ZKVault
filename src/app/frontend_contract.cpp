#include "app/frontend_contract.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::string> SplitWords(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> parts;
    std::string part;
    while (input >> part) {
        parts.push_back(part);
    }
    return parts;
}

void RequireArgumentCount(
    const std::vector<std::string>& parts,
    std::size_t expected,
    const std::string& usage) {
    if (parts.size() != expected) {
        throw std::runtime_error("usage: " + usage);
    }
}

}  // namespace

const std::vector<std::string>& CliUsageCommands() {
    static const std::vector<std::string> kCommands = {
        "zkvault init",
        "zkvault shell",
        "zkvault change-master-password",
        "zkvault add <name>",
        "zkvault get <name>",
        "zkvault update <name>",
        "zkvault delete <name>",
        "zkvault list"
    };

    return kCommands;
}

const std::vector<std::string>& ShellHelpCommands() {
    static const std::vector<std::string> kCommands = {
        "help",
        "list",
        "show <name>",
        "add <name>",
        "update <name>",
        "delete <name>",
        "change-master-password",
        "quit"
    };

    return kCommands;
}

FrontendCommand ParseShellCommand(const std::string& line) {
    const std::vector<std::string> parts = SplitWords(line);
    if (parts.empty()) {
        throw std::runtime_error("empty shell command");
    }

    const std::string& command = parts[0];

    if (command == "help") {
        RequireArgumentCount(parts, 1, "help");
        return FrontendCommand{FrontendCommandKind::kHelp, ""};
    }

    if (command == "list") {
        RequireArgumentCount(parts, 1, "list");
        return FrontendCommand{FrontendCommandKind::kList, ""};
    }

    if (command == "show") {
        RequireArgumentCount(parts, 2, "show <name>");
        return FrontendCommand{FrontendCommandKind::kShow, parts[1]};
    }

    if (command == "add") {
        RequireArgumentCount(parts, 2, "add <name>");
        return FrontendCommand{FrontendCommandKind::kAdd, parts[1]};
    }

    if (command == "update") {
        RequireArgumentCount(parts, 2, "update <name>");
        return FrontendCommand{FrontendCommandKind::kUpdate, parts[1]};
    }

    if (command == "delete") {
        RequireArgumentCount(parts, 2, "delete <name>");
        return FrontendCommand{FrontendCommandKind::kDelete, parts[1]};
    }

    if (command == "change-master-password") {
        RequireArgumentCount(parts, 1, "change-master-password");
        return FrontendCommand{FrontendCommandKind::kChangeMasterPassword, ""};
    }

    if (command == "quit" || command == "exit") {
        RequireArgumentCount(parts, 1, "quit");
        return FrontendCommand{FrontendCommandKind::kQuit, ""};
    }

    throw std::runtime_error("unknown shell command");
}

ExactConfirmationRule BuildOverwriteConfirmationRule(const std::string& name) {
    return ExactConfirmationRule{
        "Type the entry name to confirm overwrite: ",
        name,
        "entry overwrite cancelled"
    };
}

ExactConfirmationRule BuildDeletionConfirmationRule(const std::string& name) {
    return ExactConfirmationRule{
        "Type the entry name to confirm deletion: ",
        name,
        "entry deletion cancelled"
    };
}

ExactConfirmationRule BuildMasterPasswordRotationConfirmationRule() {
    return ExactConfirmationRule{
        "Type CHANGE to confirm master password rotation: ",
        "CHANGE",
        "master password rotation cancelled"
    };
}

std::string FormatStoredEntryMessage(const std::string& entry_path) {
    return "saved to " + entry_path;
}

std::string FormatUpdatedPathMessage(const std::string& path) {
    return "updated " + path;
}

std::string FormatDeletedEntryMessage(const std::string& entry_path) {
    return "deleted " + entry_path;
}
