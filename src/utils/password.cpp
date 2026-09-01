#include "password.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <sstream>
#include <iomanip>
#include <cstring>

std::string PasswordUtil::hash(const std::string& password) {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return "";
    }

    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.size(),
                       salt, sizeof(salt),
                       100000,
                       EVP_sha256(),
                       sizeof(hash),
                       hash);

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    }
    oss << "$";
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

bool PasswordUtil::verify(const std::string& password, const std::string& stored_hash) {
    size_t dollar_pos = stored_hash.find('$');
    if (dollar_pos == std::string::npos || dollar_pos != 32) {
        return false;
    }

    std::string salt_hex = stored_hash.substr(0, dollar_pos);
    std::string hash_hex = stored_hash.substr(dollar_pos + 1);

    unsigned char salt[16];
    for (int i = 0; i < 16; ++i) {
        std::string byte_str = salt_hex.substr(i * 2, 2);
        salt[i] = (unsigned char)std::stoi(byte_str, nullptr, 16);
    }

    unsigned char expected_hash[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.size(),
                       salt, sizeof(salt),
                       100000,
                       EVP_sha256(),
                       sizeof(expected_hash),
                       expected_hash);

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)expected_hash[i];
    }

    return oss.str() == hash_hex;
}
