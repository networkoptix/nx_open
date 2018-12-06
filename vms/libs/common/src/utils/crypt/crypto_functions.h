#pragma once

#include <array>

class QString;

// This module is currently used for CryptedFileStream and Encrypted Layouts.
// Some arbitrary constants are used to generate keys and hashes from passwords.

namespace nx::utils::crypto_functions {

constexpr static size_t kKeySize = 32;

// 32-byte array to hold AES key or SHA-2 hash value.
using Key = std::array<unsigned char, kKeySize>;

// Create standard Key from a password by wrapping and xoring with an arbitrary constant data.
Key adaptPassword(const char* password);

Key xorKeys(const Key& key1, const Key& key2);

// Get key hash by xoring with salt and calculating SHA-2 hash many times.
Key getKeyHash(const Key& key);

// Create hash value from a password and salt. Used for storing and password checking.
Key getSaltedPasswordHash(const QString& password, Key salt);

// Check password with (stored) salt against (stored) hash.
bool checkSaltedPassword(const QString& password, Key salt, Key hash);

// Get salt. Probably not cryptographically random.
Key getRandomSalt();

} // namespace nx::utils::crypto_functions 
