// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <tuple>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json/object.h>
#include <nx/utils/std/expected.h>

namespace nx::network::jwk {

/**
 * Key operations defined by https://datatracker.ietf.org/doc/html/rfc7517#section-4.3.
 */
NX_REFLECTION_ENUM_CLASS(KeyOp,
    sign,
    verify,
    encrypt,
    decrypt,
    wrapKey,
    unwrapKey,
    deriveKey,
    deriveBits
);

NX_REFLECTION_ENUM_CLASS(Use,
    sig,
    enc
);

/**
 * JSON web key as defined by https://datatracker.ietf.org/doc/html/rfc7517.
 * See https://datatracker.ietf.org/doc/html/rfc7517#section-4 for fields descriptions.
 */
class NX_NETWORK_API Key:
    public nx::reflect::json::Object
{
public:
    std::string kty() const;
    void setKty(const std::string& value);

    Use use() const;
    void setUse(const Use& value);

    std::vector<KeyOp> keyOps() const;
    void setKeyOps(const std::vector<KeyOp>& value);

    std::string alg() const;
    void setAlg(const std::string& value);

    std::string kid() const;
    void setKid(const std::string& value);
};

struct KeyPair
{
    Key pub;
    Key priv;
};

NX_REFLECTION_INSTRUMENT(KeyPair, (pub)(priv))

// Supported algorithms.
NX_REFLECTION_ENUM_CLASS(Algorithm,
    RS256
);

/**
 * @return A [public, private] key pair.
 */
NX_NETWORK_API nx::utils::expected<KeyPair, std::string /*error*/>
    generateKeyPairForSigning(const Algorithm& algorithm);

struct SignResult
{
    // Crypographic algorithm used to generate the signature.
    std::string alg;

    // Base64URL-encoded signature.
    std::string signature;
};

/**
 * @return algorithm that corresponds to the given key (if supported). This algorithm will be used
 * for signing with that key.
 */
NX_NETWORK_API nx::utils::expected<std::string /*alg*/, std::string /*err*/>
    getAlgorithmForSigning(const Key& key);

/**
 * Sign the message using the provided key and algorithm specified by the 'alg' property of the key.
 * Signing can fail due to unsupported key/algorithm type, or internal openssl error
 * (e.g., memory allocation failure).
 * The resulting signature in base64URL-encoded and returned.
 */
NX_NETWORK_API nx::utils::expected<SignResult, std::string /*error*/> sign(
    const std::string_view message,
    const Key& key);

struct ParsedKeyImpl;

/**
 * Opaque signing key.
 * This is a more efficient way to use a key for signing/verifying multiple messages.
 */
class NX_NETWORK_API ParsedKey
{
public:
    ParsedKey();
    ParsedKey(std::unique_ptr<ParsedKeyImpl>);
    ParsedKey(ParsedKey&& rhs);
    ~ParsedKey();

    const std::string& kid() const;
    const std::string& algorithm() const;

    ParsedKeyImpl* impl() const;

    ParsedKey& operator=(ParsedKey&& rhs);

private:
    std::unique_ptr<ParsedKeyImpl> m_impl;
};

/**
 * Prepare key for signing/verifying multiple messages in a more efficient way.
 * @return Parsed key or error text if the key is not supported.
 */
NX_NETWORK_API nx::utils::expected<ParsedKey, std::string /*error*/> parseKey(const Key& key);

/**
 * Sign message using the already parsed key.
 */
NX_NETWORK_API nx::utils::expected<SignResult, std::string /*error*/> sign(
    const std::string_view message,
    const ParsedKey& key);

/**
 * Verifies that the signature corresponds to the given message using the given key and algorithm.
 * @param algorithm Algorithm to use for verification. Used only if the key does not specify 'alg'.
 */
NX_NETWORK_API bool verify(
    const std::string_view message,
    const std::string_view signature,
    const std::string_view algorithm,
    const Key& key);

/**
 * Verifies the signature using already prepare key.
 */
NX_NETWORK_API bool verify(
    const std::string_view message,
    const std::string_view signature,
    const std::string_view algorithm,
    const ParsedKey& key);

} // namespace nx::network::jwk
