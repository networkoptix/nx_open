// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crypt.h"

#include <QtCore/QtGlobal>
#include <QtCore/QtEndian>

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/assert.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>

namespace nx::vms::crypt {


const EVP_CIPHER* toCipher(Algorithm algorithm)
{
    switch(algorithm)
    {
        case Algorithm::aes128: return EVP_aes_128_ctr();
        case Algorithm::aes256: return EVP_aes_256_ctr();
        default: return nullptr;
    }
}

/**
 * Detect AES-128 or AES-256 algorithm by cipher key.
 * In case of bytes [16..31] of the cipher key is empty
 * then it is AES-128 algorithm.
 */
Algorithm AesKey::algorithm() const
{
    const uint8_t emptyData[16] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    int result = memcmp(cipherKey.data() + 16, emptyData, sizeof(emptyData));
    if (result != 0)
        return Algorithm::aes256;
    result = memcmp(cipherKey.data(), emptyData, sizeof(emptyData));
    if (result != 0)
        return Algorithm::aes128;
    return Algorithm::none;
}

AesKey makeKey(const QString& password, Algorithm algorithm, const QByteArray& salt)
{
    const int kRrounds = 256;
    const auto passwordBytes = password.toUtf8();
    AesKey result;
    result.salt = salt;
    if (result.salt.isEmpty())
    {
        const uint64_t saltData = nx::utils::random::number<uint64_t>();
        result.salt = QByteArray::fromRawData((const char*) &saltData, sizeof(saltData));
    }

    // EVP_BytesToKey uses more long key fields up to 64 bytes. In our structures is 32 bytes only.
    // It not need more for AES-128 and AES-256 algorithm.
    std::array<uint8_t, EVP_MAX_KEY_LENGTH> key{};
    std::array<uint8_t, EVP_MAX_IV_LENGTH>  iv{};

    std::array<unsigned char, 8> saltBuffer{};
    memcpy(saltBuffer.data(), result.salt.data(), std::min((qsizetype) saltBuffer.size(), result.salt.size()));

    if (!EVP_BytesToKey(
        toCipher(algorithm),
        EVP_sha3_256(),
        saltBuffer.data(),
        (const unsigned char*) passwordBytes.data(),
        passwordBytes.size(),
        kRrounds,
        key.data(), iv.data()))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failure to create new AES key");
        return result; //< Return empty result.
    }
    memcpy(result.cipherKey.data(), key.data(), result.cipherKey.size());
    memcpy(result.ivVect.data(), iv.data(), result.ivVect.size());

    // In some platforms ivVect is generated randomly instead of data hash.
    // So, fill it manually.
    nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Sha256);
    hash.addData((const char*) result.cipherKey.data(), sizeof(result.cipherKey));
    hash.addData(result.salt);
    hash.addData(passwordBytes.data(), passwordBytes.size());
    auto ivVect = hash.result();
    NX_ASSERT(ivVect.size() >= result.ivVect.size());
    memcpy(result.ivVect.data(), ivVect.data(), result.ivVect.size());

    return result;
}

AesKey makeKey(const QString& password, const std::array<uint8_t, 16>& ivVect, const QByteArray& salt)
{
    /**
     * ivVect could be generated from aes128 or aes256. In both cases it has 128 bit length.
     * Try to generate hash for both algorithms and check that hash it matches.
    */
    const auto key128 = makeKey(password, Algorithm::aes128, salt);
    if (memcmp(key128.ivVect.data(), ivVect.data(), sizeof(ivVect)) == 0)
        return key128;

    const auto key256 = makeKey(password, Algorithm::aes256, salt);
    if (memcmp(key256.ivVect.data(), ivVect.data(), sizeof(ivVect)) == 0)
        return key256;

    return AesKey();
}

AesKey makeKey(const QString& password, const std::vector<uint8_t>& encryptionData)
{
    const auto [key, nonce] = fromEncryptionData(encryptionData);
    return makeKey(password, key.ivVect, key.salt);
}

std::vector<uint8_t> toEncryptionData(const AesKey& key, uint64_t nonce)
{
    QByteArray result;
    result.append((const char*) key.ivVect.data(), key.ivVect.size());
    nonce = qToBigEndian((quint64) nonce);
    result.append((const char*) &nonce, sizeof(nonce));
    result.append(key.salt);
    return std::vector<uint8_t>(result.begin(), result.end());
}

std::pair<AesKey, uint64_t> fromEncryptionData(const std::vector<uint8_t>& encryptionData)
{
    AesKey key;
    if (encryptionData.size() < 24)
        return {key, 0};

    memcpy(key.ivVect.data(), encryptionData.data(), key.ivVect.size());
    uint64_t nonce;
    memcpy(&nonce, encryptionData.data() + 16, sizeof(nonce));
    nonce = qFromBigEndian((quint64) nonce);
    static const int kSaltOffset = key.ivVect.size() + sizeof(nonce);
    if (encryptionData.size() > kSaltOffset)
        key.salt = QByteArray((const char*) encryptionData.data() + kSaltOffset, encryptionData.size() - kSaltOffset);

    return { key, nonce };
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AesKey, (json), AesKey_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AesKeyWithTime, (json), AesKeyWithTime_Fields)

} // namespace nx::vms::crypt
