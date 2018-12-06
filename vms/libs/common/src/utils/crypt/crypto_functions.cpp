#include "crypto_functions.h"

#include <random>

#include <openssl/evp.h>

#include <nx/utils/log/assert.h>

namespace nx::utils::crypto_functions {

const Key kHashSalt {
    0x89, 0x1e, 0xed, 0x37, 0xb9, 0x5f, 0xcc, 0x9f, 0xd0, 0x3b, 0x29, 0x7e, 0x59, 0x6d, 0xed, 0xe,
    0x9c, 0x3a, 0x25, 0x2f, 0xf8, 0xb8, 0xc8, 0x98, 0x1f, 0xa3, 0xbb, 0x31, 0x67, 0x10, 0x7a, 0x52
};

const Key kPasswordSalt {
    0x31, 0xc6, 0x82, 0x69, 0xbc, 0x8d, 0xf7, 0x91, 0x2e, 0xd8, 0x2d, 0xd7, 0xbf, 0x5b, 0x99, 0xe,
    0x83, 0xc6, 0xe9, 0x9e, 0xdf, 0x69, 0x5e, 0x4e, 0x8b, 0xa5, 0xd7, 0xbc, 0x8b, 0xb3, 0xf2, 0x6
};

constexpr int kHashCount = 4242; //< Arbitrary value.


Key adaptPassword(const char* password)
{
    static char zero = 0;
    if (!password) //< Just in case - we need password[0] to be valid.
        password = &zero;

    const size_t passLength = std::max((size_t) 1, strlen(password));
    Key key = kPasswordSalt;
    for (size_t i = 0; i < passLength; i++)
        key[i % kKeySize] ^= password[i] ^ (char) (i & 0xFF); 
    return key;
}

Key xorKeys(const Key& key1, const Key& key2)
{
    Key key;
    for (size_t i = 0; i < kKeySize; i++)
        key[i] = key1[i] ^ key2[i];
    return key;
}

Key getKeyHash(const Key& key)
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
    NX_ASSERT(mdctx);
    auto result = EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    NX_ASSERT(result);

    Key xored = key;
    for (int i = 0; i < kHashCount; i++)
    {
        xored = xorKeys(xored, kHashSalt);
        result = EVP_DigestUpdate(mdctx, xored.data(), kKeySize);
    }
    NX_ASSERT(result);

    Key hash;
    unsigned int len;
    result = EVP_DigestFinal_ex(mdctx, hash.data(), &len);
    NX_ASSERT(result);
    NX_ASSERT(len == kKeySize);

    EVP_MD_CTX_destroy(mdctx);

    return hash;
}

Key getSaltedPasswordHash(const QString& password, Key salt)
{
    return getKeyHash(xorKeys(salt, adaptPassword(password.toUtf8())));
}

bool checkSaltedPassword(const QString& password, Key salt, Key hash)
{
    return getKeyHash(xorKeys(salt, adaptPassword(password.toUtf8()))) == hash;
}

// This function generates salt; need not to be cryptographically strong random.
Key getRandomSalt()
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 255);
    generator.seed(time(nullptr));

    Key key;
    for (size_t i = 0; i < kKeySize; i++)
        key[i] = distribution(generator);

    return key;
}

} // namespace nx::utils::crypto_functions
