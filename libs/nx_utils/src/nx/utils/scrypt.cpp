// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scrypt.h"

#include "log/log.h"
#include "type_utils.h"
#include "string.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>

namespace nx::scrypt {

bool Options::operator==(const Options& rhs) const
{
    return r == rhs.r && N == rhs.N && p == rhs.p && keySize == rhs.keySize;
}

static auto makeContext(const Options& options)
{
    auto context = nx::utils::wrapUnique(
        EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, NULL),
        &EVP_PKEY_CTX_free);

    if (EVP_PKEY_derive_init(context.get()) <= 0)
        throw Exception("Unable to init SCrypt context");

    if (EVP_PKEY_CTX_set_scrypt_N(context.get(), options.N) <= 0)
        throw Exception("Unable to set SCrypt N (CPU/memory cost)");

    if (EVP_PKEY_CTX_set_scrypt_r(context.get(), options.r) <= 0)
        throw Exception("Unable to set SCrypt r (block size)");

    if (EVP_PKEY_CTX_set_scrypt_p(context.get(), options.p) <= 0)
        throw Exception("Unable to set SCrypt p (parallelization)");

    return context;
}

void Options::validateOrThrow() const
{
    if (keySize == 0)
        throw Exception("SCrypt keysize should be positive");

    makeContext(*this);
}

bool Options::isValid() const
{
    try
    {
        makeContext(*this);
        return true;
    }
    catch(const Exception& e)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "Wrong options: %1", e);
        return false;
    }
}

QString Options::toString() const
{
    return NX_FMT(R"json({"r": %1, "N": %2, "p": %3, "keySize": %4})json", r, N, p, keySize);
}

std::string encodeOrThrow(
    const std::string& password,
    const std::string& salt,
    const Options& options)
{
    NX_TRACE(NX_SCOPE_TAG, "Calculating scrypt");

    const auto context = makeContext(options);

    if (EVP_PKEY_CTX_set1_pbe_pass(context.get(), password.c_str(), password.size()) <= 0)
        throw Exception("Unable to set SCrypt password");

    if (EVP_PKEY_CTX_set1_scrypt_salt(context.get(), salt.c_str(), salt.size()) <= 0)
        throw Exception("Unable to set SCrypt salt");

    size_t size = options.keySize;
    std::vector<char> key(size, '\0');
    if (EVP_PKEY_derive(context.get(), (unsigned char*) key.data(), &size) <= 0 || size == 0)
        throw Exception("Unable to extract SCrypt key");

    return nx::utils::toHex(std::string_view(key.data(), size));
}

std::optional<std::string> encode(
    const std::string& password,
    const std::string& salt,
    const Options& options)
{
    try
    {
        return encodeOrThrow(password, salt, options);
    }
    catch(const Exception& e)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Unable to encrypt key: %1", e);
        return std::nullopt;
    }
}

} // nx::scrypt
