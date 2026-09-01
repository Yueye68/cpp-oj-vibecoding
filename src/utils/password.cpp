#include "password.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <crypt.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <errno.h>

std::string PasswordUtil::hash(const std::string& password) {
    unsigned char random_bytes[16];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        return "";
    }

    char salt_str[CRYPT_GENSALT_OUTPUT_SIZE];
    char* gensalt_result = crypt_gensalt_rn("$2b$10$", 10, reinterpret_cast<const char*>(random_bytes), 16, salt_str, sizeof(salt_str));
    if (gensalt_result == nullptr) {
        return "";
    }

    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    char* result = crypt_r(password.c_str(), gensalt_result, &data);
    if (result == nullptr || result[0] == '*') {
        return "";
    }

    return std::string(result);
}

bool PasswordUtil::verify(const std::string& password, const std::string& stored_hash) {
    if (stored_hash.empty() || stored_hash[0] == '*') {
        return false;
    }

    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    char* result = crypt_r(password.c_str(), stored_hash.c_str(), &data);
    if (result == nullptr) {
        return false;
    }

    return strcmp(result, stored_hash.c_str()) == 0;
}