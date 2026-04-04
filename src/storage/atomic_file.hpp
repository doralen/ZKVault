#pragma once

#include <filesystem>
#include <string>

void WriteFileAtomically(
    const std::filesystem::path& path,
    const std::string& contents);
