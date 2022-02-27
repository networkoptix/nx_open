// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QLatin1String>

#include <nx/fusion/model_functions_fwd.h>

extern "C" {
    #include <openssl/evp.h>
}

#include <nx/reflect/instrument.h>

namespace nx::vms::crypt {

enum class Algorithm
{
    none,
    aes128,
    aes256
};

struct NX_VMS_COMMON_API AesKey
{
    Algorithm algorithm() const;
    bool isEmpty() const { return algorithm() == Algorithm::none; }

    /**%apidoc[unused]. Hidden in API */
    std::array<uint8_t, 32> cipherKey = {};

    /**%apidoc:array one of the keys to decrypt archive. The second key 'cipherKey' is hidden in API and can't be read. */
    std::array<uint8_t, 16> ivVect = {};

    /**%apidoc[unused] salt. Hidden in API. */
    QByteArray salt;

    bool operator==(const AesKey& other) const = default;
};
#define AesKey_Fields (cipherKey)(ivVect)(salt)

struct AesKeyWithTime: public AesKey
{
    AesKeyWithTime() = default;
    AesKeyWithTime(const AesKey& value): AesKey(value) {}

    /**%apidoc[opt] issue day. Can be 0. The key with maximum issueDateUs is used to encrypt archive */
    std::chrono::microseconds issueDateUs {};

    bool operator==(const AesKeyWithTime& other) const = default;
};
#define AesKeyWithTime_Fields AesKey_Fields(issueDateUs)

/**
    * @return AesKey from user password
    */
NX_VMS_COMMON_API AesKey makeKey(const QString& password, Algorithm algorithm, const QByteArray& salt);

/**
    * Try to create AesKey by known password and existing ivVect.
    * This function is used to find cipher key by known ivVect and password.
    */
NX_VMS_COMMON_API AesKey makeKey(const QString& password, const std::vector<uint8_t>& encryptionData);

NX_VMS_COMMON_API std::vector<uint8_t> toEncryptionData(const AesKey& key, uint64_t nonce);
NX_VMS_COMMON_API std::pair<AesKey, uint64_t> fromEncryptionData(const std::vector<uint8_t>& encryptionData);


NX_VMS_COMMON_API const EVP_CIPHER* toCipher(Algorithm algorithm);

QN_FUSION_DECLARE_FUNCTIONS(AesKey, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(AesKeyWithTime, (json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(AesKeyWithTime, AesKeyWithTime_Fields)

} // namespace nx::vms::crypt
