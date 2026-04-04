#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include <openssl/crypto.h>

#include "crypto/aead.hpp"
#include "crypto/hex.hpp"
#include "crypto/kdf.hpp"
#include "crypto/random.hpp"
#include "model/encrypted_entry_file.hpp"
#include "model/master_key_file.hpp"
#include "model/password_entry.hpp"
#include "storage/master_key_storage.hpp"
#include "storage/json_storage.hpp"

namespace {

void CleanseString(std::string& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

void CleanseBytes(std::vector<unsigned char>& value) {
    if (!value.empty()) {
        OPENSSL_cleanse(value.data(), value.size());
    }
}

std::string NowIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string ReadSecret(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    if (!isatty(STDIN_FILENO)) {
        std::string secret;
        std::getline(std::cin, secret);
        return secret;
    }

    termios old_settings{};
    if (tcgetattr(STDIN_FILENO, &old_settings) != 0) {
        throw std::runtime_error("failed to read terminal settings");
    }

    termios new_settings = old_settings;
    new_settings.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != 0) {
        throw std::runtime_error("failed to disable terminal echo");
    }

    std::string secret;
    std::getline(std::cin, secret);
    std::cout << '\n';

    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_settings) != 0) {
        throw std::runtime_error("failed to restore terminal settings");
    }

    return secret;
}

std::string ReadLine(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string value;
    std::getline(std::cin, value);
    return value;
}

void RequireExactConfirmation(
    const std::string& prompt,
    const std::string& expected,
    const std::string& mismatch_error) {
    const std::string value = ReadLine(prompt);
    if (value != expected) {
        throw std::runtime_error(mismatch_error);
    }
}

std::string ReadConfirmedSecret(
    const std::string& prompt,
    const std::string& confirm_prompt,
    const std::string& mismatch_error) {
    std::string secret = ReadSecret(prompt);
    std::string confirm_secret = ReadSecret(confirm_prompt);

    if (secret != confirm_secret) {
        CleanseString(secret);
        CleanseString(confirm_secret);
        throw std::runtime_error(mismatch_error);
    }

    CleanseString(confirm_secret);
    return secret;
}

std::vector<unsigned char> UnlockDataKey(const std::string& master_password) {
    const MasterKeyFile master_key_file = LoadMasterKeyFile();

    std::vector<unsigned char> salt = HexToBytes(master_key_file.salt);
    std::vector<unsigned char> wrap_iv = HexToBytes(master_key_file.wrap_iv);
    std::vector<unsigned char> encrypted_dek =
        HexToBytes(master_key_file.encrypted_dek);
    std::vector<unsigned char> auth_tag =
        HexToBytes(master_key_file.auth_tag);

    std::vector<unsigned char> kek =
        DeriveKeyScrypt(master_password, salt, 32);
    std::vector<unsigned char> dek =
        DecryptAes256Gcm(kek, wrap_iv, encrypted_dek, auth_tag);

    CleanseBytes(kek);
    CleanseBytes(salt);
    CleanseBytes(wrap_iv);
    CleanseBytes(encrypted_dek);
    CleanseBytes(auth_tag);

    return dek;
}

MasterKeyFile CreateMasterKeyFile(
    const std::string& master_password,
    const std::vector<unsigned char>& dek) {
    std::vector<unsigned char> salt = GenerateRandomBytes(16);
    std::vector<unsigned char> kek = DeriveKeyScrypt(master_password, salt, 32);
    const AeadCiphertext wrapped_dek = EncryptAes256Gcm(kek, dek);

    MasterKeyFile master_key_file{
        1,
        "scrypt",
        BytesToHex(salt),
        BytesToHex(wrapped_dek.iv),
        BytesToHex(wrapped_dek.ciphertext),
        BytesToHex(wrapped_dek.auth_tag)
    };

    CleanseBytes(kek);
    CleanseBytes(salt);
    return master_key_file;
}

EncryptedEntryFile EncryptPasswordEntry(
    const PasswordEntry& entry,
    const std::vector<unsigned char>& dek) {
    const json serialized = entry;
    std::string plaintext = serialized.dump();
    std::vector<unsigned char> plaintext_bytes(plaintext.begin(), plaintext.end());
    const AeadCiphertext encrypted = EncryptAes256Gcm(dek, plaintext_bytes);
    CleanseString(plaintext);
    CleanseBytes(plaintext_bytes);

    return EncryptedEntryFile{
        1,
        BytesToHex(encrypted.iv),
        BytesToHex(encrypted.ciphertext),
        BytesToHex(encrypted.auth_tag)
    };
}

PasswordEntry DecryptPasswordEntry(
    const EncryptedEntryFile& file,
    const std::vector<unsigned char>& dek) {
    std::vector<unsigned char> iv = HexToBytes(file.data_iv);
    std::vector<unsigned char> ciphertext = HexToBytes(file.ciphertext);
    std::vector<unsigned char> auth_tag = HexToBytes(file.auth_tag);
    std::vector<unsigned char> plaintext_bytes =
        DecryptAes256Gcm(dek, iv, ciphertext, auth_tag);
    std::string plaintext(plaintext_bytes.begin(), plaintext_bytes.end());
    const json serialized = json::parse(plaintext);
    const PasswordEntry entry = serialized.get<PasswordEntry>();
    CleanseBytes(iv);
    CleanseBytes(ciphertext);
    CleanseBytes(auth_tag);
    CleanseBytes(plaintext_bytes);
    CleanseString(plaintext);
    return entry;
}

void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  zkvault init\n";
    std::cout << "  zkvault change-master-password\n";
    std::cout << "  zkvault add <name>\n";
    std::cout << "  zkvault get <name>\n";
    std::cout << "  zkvault update <name>\n";
    std::cout << "  zkvault delete <name>\n";
    std::cout << "  zkvault list\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            PrintUsage();
            return 1;
        }

        std::string command = argv[1];

        if (command == "init") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            std::string master_password = ReadConfirmedSecret(
                "Master password: ",
                "Confirm master password: ",
                "master passwords do not match");
            std::vector<unsigned char> dek = GenerateRandomBytes(32);
            const MasterKeyFile master_key_file =
                CreateMasterKeyFile(master_password, dek);

            SaveMasterKeyFile(master_key_file);
            CleanseString(master_password);
            CleanseBytes(dek);
            std::cout << "initialized .zkv_master\n";
            return 0;
        }

        if (command == "change-master-password") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            RequireExactConfirmation(
                "Type CHANGE to confirm master password rotation: ",
                "CHANGE",
                "master password rotation cancelled");
            std::string current_master_password =
                ReadSecret("Current master password: ");
            std::vector<unsigned char> dek = UnlockDataKey(current_master_password);
            std::string new_master_password = ReadConfirmedSecret(
                "New master password: ",
                "Confirm new master password: ",
                "new master passwords do not match");

            const MasterKeyFile master_key_file =
                CreateMasterKeyFile(new_master_password, dek);
            OverwriteMasterKeyFile(master_key_file);
            CleanseString(current_master_password);
            CleanseString(new_master_password);
            CleanseBytes(dek);
            std::cout << "updated .zkv_master\n";
            return 0;
        }

        if (command == "add" || command == "update") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            const bool entry_exists = PasswordEntryExists(argv[2]);
            if (command == "add" && entry_exists) {
                throw std::runtime_error("entry already exists");
            }

            if (command == "update" && !entry_exists) {
                throw std::runtime_error("entry does not exist");
            }

            if (command == "update") {
                RequireExactConfirmation(
                    "Type the entry name to confirm overwrite: ",
                    argv[2],
                    "entry overwrite cancelled");
            }

            std::string master_password = ReadSecret("Master password: ");
            std::vector<unsigned char> dek = UnlockDataKey(master_password);

            std::string now = NowIso8601();
            std::string created_at = now;

            if (command == "update") {
                PasswordEntry old_entry =
                    DecryptPasswordEntry(LoadEncryptedEntryFile(argv[2]), dek);
                if (!old_entry.created_at.empty()) {
                    created_at = old_entry.created_at;
                }
                CleanseString(old_entry.password);
                CleanseString(old_entry.note);
            }

            PasswordEntry entry{
                argv[2],
                ReadSecret("Entry password: "),
                ReadLine("Note: "),
                created_at,
                now
            };

            const EncryptedEntryFile encrypted = EncryptPasswordEntry(entry, dek);
            SaveEncryptedEntryFile(entry.name, encrypted);
            CleanseString(master_password);
            CleanseString(entry.password);
            CleanseString(entry.note);
            CleanseBytes(dek);
            std::cout << (command == "add" ? "saved to " : "updated ")
                      << "data/" << entry.name << ".zkv\n";
            return 0;
        }

        if (command == "get") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            std::string master_password = ReadSecret("Master password: ");
            std::vector<unsigned char> dek = UnlockDataKey(master_password);
            const EncryptedEntryFile encrypted = LoadEncryptedEntryFile(argv[2]);
            PasswordEntry entry = DecryptPasswordEntry(encrypted, dek);
            json serialized = entry;
            std::string output = serialized.dump(2);
            CleanseString(master_password);
            CleanseBytes(dek);
            std::cout << output << '\n';
            CleanseString(entry.password);
            CleanseString(entry.note);
            CleanseString(output);
            return 0;
        }

        if (command == "delete") {
            if (argc != 3) {
                PrintUsage();
                return 1;
            }

            RequireExactConfirmation(
                "Type the entry name to confirm deletion: ",
                argv[2],
                "entry deletion cancelled");
            DeletePasswordEntry(argv[2]);
            std::cout << "deleted data/" << argv[2] << ".zkv\n";
            return 0;
        }

        if (command == "list") {
            if (argc != 2) {
                PrintUsage();
                return 1;
            }

            for (const std::string& name : ListPasswordEntries()) {
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
