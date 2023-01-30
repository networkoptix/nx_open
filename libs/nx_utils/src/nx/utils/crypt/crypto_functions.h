// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <cstddef>

class QString;

// This module is currently used for CryptedFileStream and Encrypted Layouts.
// Some arbitrary constants are used to generate keys and hashes from passwords.

namespace nx::crypt {

constexpr static size_t kKeySize = 32;

// 32-byte array to hold AES key or SHA-2 hash value.
using Key = std::array<unsigned char, kKeySize>;

// Create standard Key from a password by wrapping and xoring with an arbitrary constant data.
NX_UTILS_API Key adaptPassword(const char* password);

NX_UTILS_API Key xorKeys(const Key& key1, const Key& key2);

// Get key hash by xoring with salt and calculating SHA-2 hash many times.
NX_UTILS_API Key getKeyHash(const Key& key);

// Create hash value from a password and salt. Used for storing and password checking.
NX_UTILS_API Key getSaltedPasswordHash(const QString& password, Key salt);

// Check password with (stored) salt against (stored) hash.
NX_UTILS_API bool checkSaltedPassword(const QString& password, Key salt, Key hash);

// Get salt. Probably not cryptographically random.
NX_UTILS_API Key getRandomSalt();

} // namespace nx::crypt
