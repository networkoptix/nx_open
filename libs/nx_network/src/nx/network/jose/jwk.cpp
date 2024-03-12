// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jwk.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <nx/utils/base64.h>
#include <nx/utils/uuid.h>

namespace nx::network::jwk {

std::string Key::kty() const
{
    return get<std::string>("kty").value_or(std::string());
}

void Key::setKty(const std::string& value)
{
    set("kty", value);
}

Use Key::use() const
{
    return get<Use>("use").value_or(Use::sig);
}

void Key::setUse(const Use& value)
{
    set("use", value);
}

std::vector<KeyOp> Key::keyOps() const
{
    return get<std::vector<KeyOp>>("key_ops").value_or(std::vector<KeyOp>());
}

void Key::setKeyOps(const std::vector<KeyOp>& value)
{
    set("key_ops", value);
}

std::string Key::alg() const
{
    return get<std::string>("alg").value_or(std::string());
}

void Key::setAlg(const std::string& value)
{
    set("alg", value);
}

std::string Key::kid() const
{
    return get<std::string>("kid").value_or(std::string());
}

void Key::setKid(const std::string& value)
{
    set("kid", value);
}

//--------------------------------------------------------------------------------------------------
// RS256 assymetric key signing and verification.

static std::string bnToBase64Url(const BIGNUM* bn)
{
    int len = BN_num_bytes(bn);
    unsigned char* buffer = new unsigned char[len];

    BN_bn2bin(bn, buffer);
    std::string b64 = nx::utils::toBase64Url(std::string_view((const char*) buffer, len));

    delete[] buffer;
    return b64;
}

static RSA* jwkToRsa(const Key& jwkKey)
{
    RSA* rsa = RSA_new();
    BIGNUM* n = BN_new();
    BIGNUM* e = BN_new();

    if (!jwkKey.contains("n") || !jwkKey.contains("e"))
        return nullptr;

    // Convert Base64URL encoded JWK parameters to BIGNUM
    std::string nBin = nx::utils::fromBase64Url(*jwkKey.get<std::string>("n"));
    BN_bin2bn((const unsigned char*) nBin.data(), nBin.size(), n);

    std::string eBin = nx::utils::fromBase64Url(*jwkKey.get<std::string>("e"));
    BN_bin2bn((const unsigned char*) eBin.data(), eBin.size(), e);

    if (jwkKey.contains("d"))
    {
        BIGNUM* d = BN_new();
        std::string dBin = nx::utils::fromBase64Url(*jwkKey.get<std::string>("d"));
        BN_bin2bn((const unsigned char*) dBin.data(), dBin.size(), d);

        RSA_set0_key(rsa, n, e, d);
    }
    else
    {
        RSA_set0_key(rsa, n, e, nullptr);
    }

    return rsa;
}

nx::utils::expected<KeyPair, std::string /*error*/>
    generateKeyPairForSigning(const Algorithm& /*algorithm*/)
{
    // TODO: add support for algorithm.

    // Key size for RSA, typically 2048 or 3072 bits for RS256
    const int keySize = 2048;

    // Generate RSA key
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4); // 65537 as the public exponent

    RSA_generate_key_ex(rsa, keySize, bn, nullptr);

    // Extract key components
    const BIGNUM* rsa_n, * rsa_e, * rsa_d;
    RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);

    // Convert BIGNUMs to Base64URL encoded strings
    std::string n_b64 = bnToBase64Url(rsa_n);
    std::string e_b64 = bnToBase64Url(rsa_e);
    std::string d_b64 = bnToBase64Url(rsa_d);

    const std::string kid = nx::Uuid::createUuid().toSimpleStdString();

    // Populate JWK structures
    Key publicKey, privateKey;
    // Public key components
    publicKey.setKty("RSA");
    publicKey.setUse(Use::sig);
    publicKey.setAlg("RS256");
    publicKey.setKid(kid);
    publicKey.set("n", n_b64);
    publicKey.set("e", e_b64);
    publicKey.setKeyOps({KeyOp::verify});

    // Private key components (includes public components as well)
    privateKey.setKty("RSA");
    privateKey.setUse(Use::sig);
    privateKey.setAlg("RS256");
    privateKey.setKid(kid);
    privateKey.set("n", n_b64);
    privateKey.set("e", e_b64);
    privateKey.set("d", d_b64);
    privateKey.setKeyOps({KeyOp::sign});

    // Clean up
    RSA_free(rsa);
    BN_free(bn);

    // Return the key pair
    return KeyPair{.pub = publicKey, .priv = privateKey};
}

//-------------------------------------------------------------------------------------------------

struct ParsedKeyImpl
{
    const std::string algorithm;
    const std::string kid;

    EVP_PKEY* key = nullptr;
    const std::string binaryKey;

    ParsedKeyImpl(std::string algorithm, std::string kid, EVP_PKEY* key):
        algorithm(std::move(algorithm)),
        kid(std::move(kid)),
        key(key)
    {
    }

    ParsedKeyImpl(std::string algorithm, std::string kid, std::string binaryKey):
        algorithm(std::move(algorithm)),
        kid(std::move(kid)),
        binaryKey(std::move(binaryKey))
    {
    }

    ~ParsedKeyImpl()
    {
        if (key)
            EVP_PKEY_free(key);
    }

    ParsedKeyImpl(const ParsedKeyImpl&) = delete;
    ParsedKeyImpl(ParsedKeyImpl&&) = delete;
};

ParsedKey::ParsedKey() = default;

ParsedKey::ParsedKey(std::unique_ptr<ParsedKeyImpl> impl):
    m_impl(std::move(impl))
{
}

ParsedKey::ParsedKey(ParsedKey&& rhs)
{
    m_impl = std::exchange(rhs.m_impl, nullptr);
}

ParsedKey::~ParsedKey() = default;

const std::string& ParsedKey::kid() const
{
    return m_impl->kid;
}

const std::string& ParsedKey::algorithm() const
{
    return m_impl->algorithm;
}

ParsedKeyImpl* ParsedKey::impl() const
{
    return m_impl.get();
}

ParsedKey& ParsedKey::operator=(ParsedKey&& rhs)
{
    m_impl = std::exchange(rhs.m_impl, nullptr);
    return *this;
}

nx::utils::expected<ParsedKey, std::string /*error*/> parseKey(const Key& key)
{
    if (key.alg() == "RS256")
    {
        RSA* rsa = jwkToRsa(key);
        if (!rsa)
            return nx::utils::unexpected<std::string>("Cannot convert JWK to RSA");

        EVP_PKEY* signing_key = EVP_PKEY_new();
        EVP_PKEY_assign_RSA(signing_key, rsa);

        return ParsedKey(std::make_unique<ParsedKeyImpl>(key.alg(), key.kid(), signing_key));
    }
    else if (key.alg() == "HS256")
    {
        const auto k = key.get<std::string>("k");
        if (!k)
            return nx::utils::unexpected<std::string>("Inappropriate key for HS256 signing: no 'k' attribute");

        // Decode the key from Base64URL.
        return ParsedKey(std::make_unique<ParsedKeyImpl>(key.alg(), key.kid(), nx::utils::fromBase64Url(*k)));
    }

    return nx::utils::unexpected<std::string>("alg '" + key.alg() + "' is not supported");
}

//-------------------------------------------------------------------------------------------------

nx::utils::expected<SignResult, std::string /*error*/> rs256Sign(
    const std::string_view message,
    const ParsedKey& key);

bool rs256Verify(
    const std::string_view message,
    const std::string_view signature,
    const ParsedKey& key);

nx::utils::expected<SignResult, std::string /*error*/> hs256Sign(
    const std::string_view message,
    const ParsedKey& key);

bool hs256Verify(
    const std::string_view message,
    const std::string_view signature,
    const ParsedKey& key);

//-------------------------------------------------------------------------------------------------

nx::utils::expected<std::string /*alg*/, std::string /*err*/> getAlgorithmForSigning(const Key& key)
{
    const auto alg = key.alg();
    if (alg.empty())
        return nx::utils::unexpected<std::string>("Key must specify alg to use for signing");

    if (alg == "RS256" || alg == "HS256")
        return alg;
    else
        return nx::utils::unexpected<std::string>("alg '" + alg + "' is not supported for signing");
}

nx::utils::expected<SignResult, std::string /*error*/> sign(
    const std::string_view message,
    const ParsedKey& key)
{
    if (key.impl()->algorithm == "RS256")
        return rs256Sign(message, key);
    else if (key.impl()->algorithm == "HS256")
        return hs256Sign(message, key);
    else
        return nx::utils::unexpected<std::string>("alg '" + key.impl()->algorithm +
            "' is not supported for signing");
}

nx::utils::expected<SignResult, std::string /*error*/> sign(
    const std::string_view message,
    const Key& key)
{
    const auto alg = key.alg();
    if (alg.empty())
        return nx::utils::unexpected<std::string>("Key must specify alg to use for signing");

    const auto keyOps = key.keyOps();
    if (std::count(keyOps.begin(), keyOps.end(), KeyOp::sign) == 0)
    {
        return nx::utils::unexpected<std::string>("Cannot proceed with signing given a key "
            "that does not specify 'sign' operation");
    }

    const auto parsedKey = parseKey(key);
    if (!parsedKey)
    {
        return nx::utils::unexpected<std::string>("Cannot parse the key for signing: " +
            parsedKey.error());
    }

    return sign(message, *parsedKey);
}

bool verify(
    const std::string_view message,
    const std::string_view signature,
    const std::string_view algorithm,
    const ParsedKey& key)
{
    std::string alg(algorithm);
    if (!key.impl()->algorithm.empty())
        alg = key.impl()->algorithm;

    if (alg == "RS256")
        return rs256Verify(message, signature, key);
    else if (alg == "HS256")
        return hs256Verify(message, signature, key);
    else
        return false;
}

bool verify(
    const std::string_view message,
    const std::string_view signature,
    const std::string_view algorithm,
    const Key& key)
{
    const auto parsedKey = parseKey(key);
    if (!parsedKey)
        return false;

    return verify(message, signature, algorithm, *parsedKey);
}

//-------------------------------------------------------------------------------------------------
// RS256 asymetric key signing and verification.

nx::utils::expected<SignResult, std::string /*error*/> rs256Sign(
    const std::string_view message,
    const ParsedKey& key)
{
    unsigned char signature[256]; //< Size depends on RSA key size.
    unsigned int sig_len = 0;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_SignInit(md_ctx, EVP_sha256());
    EVP_SignUpdate(md_ctx, message.data(), message.size());
    EVP_SignFinal(md_ctx, signature, &sig_len, key.impl()->key);
    EVP_MD_CTX_free(md_ctx);

    // Convert signature to Base64URL or required format.
    std::string encoded_sig =
        nx::utils::toBase64Url(std::string_view((const char*) signature, sig_len));

    return SignResult{.alg = "RS256", .signature = encoded_sig};
}

bool rs256Verify(
    const std::string_view message,
    const std::string_view signature,
    const ParsedKey& key)
{
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_VerifyInit(md_ctx, EVP_sha256());
    EVP_VerifyUpdate(md_ctx, message.data(), message.size());

    std::string decoded_signature = nx::utils::fromBase64Url(signature);
    int verify_status = EVP_VerifyFinal(
        md_ctx, (const unsigned char*) decoded_signature.data(), decoded_signature.size(), key.impl()->key);

    EVP_MD_CTX_free(md_ctx);

    return verify_status == 1;
}

//--------------------------------------------------------------------------------------------------
// HS256 symetric key signing and verification.

nx::utils::expected<SignResult, std::string /*error*/> hs256Sign(
    const std::string_view message,
    const ParsedKey& key)
{
    // Buffer to store the HMAC result
    auto result = std::make_unique<unsigned char[]>(EVP_MAX_MD_SIZE);
    unsigned int result_len = 0;

    // Create the HMAC.
    HMAC(EVP_sha256(), key.impl()->binaryKey.data(), key.impl()->binaryKey.size(),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(),
         result.get(), &result_len);

    // Convert the result to a Base64URL encoded string.
    std::string signature = nx::utils::toBase64Url(
        std::string_view(reinterpret_cast<const char*>(result.get()), result_len));

    return SignResult{.alg = "HS256", .signature = signature};
}

bool hs256Verify(
    const std::string_view message,
    const std::string_view signature,
    const ParsedKey& key)
{
    // Sign the message with the same key
    auto signResult = hs256Sign(message, key);
    if (!signResult)
        return false;

    // Compare the provided signature with the newly generated one
    return signature == signResult->signature;
}

} // namespace nx::network::jwk
