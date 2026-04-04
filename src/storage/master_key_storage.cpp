#include "storage/master_key_storage.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "storage/atomic_file.hpp"

namespace {

const std::filesystem::path kMasterKeyPath = ".zkv_master";

void WriteMasterKeyFile(const MasterKeyFile& file) {
    json serialized = file;
    WriteFileAtomically(kMasterKeyPath, serialized.dump(2));
}

}  // namespace

void SaveMasterKeyFile(const MasterKeyFile& file) {
    if (std::filesystem::exists(kMasterKeyPath)) {
        throw std::runtime_error(".zkv_master already exists");
    }

    WriteMasterKeyFile(file);
}

void OverwriteMasterKeyFile(const MasterKeyFile& file) {
    WriteMasterKeyFile(file);
}

MasterKeyFile LoadMasterKeyFile() {
    std::ifstream input(kMasterKeyPath);
    if (!input) {
        throw std::runtime_error("failed to open .zkv_master");
    }

    json serialized;
    input >> serialized;

    return serialized.get<MasterKeyFile>();
}
