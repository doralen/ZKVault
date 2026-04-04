#include "storage/json_storage.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "storage/atomic_file.hpp"

namespace {

void ValidateEntryName(std::string_view name) {
    if (name.empty()) {
        throw std::runtime_error("entry name must not be empty");
    }

    if (name == "." || name == "..") {
        throw std::runtime_error("entry name must not be '.' or '..'");
    }

    for (const unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
            continue;
        }

        throw std::runtime_error(
            "entry name may only contain letters, digits, '.', '-' and '_'");
    }
}

std::filesystem::path EntryPath(const std::string& name) {
    ValidateEntryName(name);
    return std::filesystem::path("data") / (name + ".zkv");
}

}  // namespace

void SaveEncryptedEntryFile(const std::string& name, const EncryptedEntryFile& file) {
    json serialized = file;
    WriteFileAtomically(EntryPath(name), serialized.dump(2));
}

EncryptedEntryFile LoadEncryptedEntryFile(const std::string& name) {
    std::ifstream input(EntryPath(name));
    if (!input) {
        throw std::runtime_error("failed to open input file");
    }

    json serialized;
    input >> serialized;

    return serialized.get<EncryptedEntryFile>();
}

bool PasswordEntryExists(const std::string& name) {
    return std::filesystem::exists(EntryPath(name));
}

void DeletePasswordEntry(const std::string& name) {
    const std::filesystem::path path = EntryPath(name);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("entry does not exist");
    }

    if (!std::filesystem::remove(path)) {
        throw std::runtime_error("failed to delete entry");
    }
}

std::vector<std::string> ListPasswordEntries() {
    std::vector<std::string> names;
    const std::filesystem::path data_dir = "data";

    if (!std::filesystem::exists(data_dir)) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::filesystem::path path = entry.path();
        if (path.extension() == ".zkv") {
            names.push_back(path.stem().string());
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}
