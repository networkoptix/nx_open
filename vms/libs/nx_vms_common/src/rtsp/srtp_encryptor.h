// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <nx/utils/byte_array.h>
#include <srtp2/srtp.h>

namespace nx::rtsp {

constexpr int kSrtpAes128KeyLen = 16;
constexpr int kSrtpAes256KeyLen = 32;
constexpr int kSrtpSaltLen = 14;
constexpr int kSrtpAeadSaltLen = 12;
constexpr int kSrtpHmacSha1KeyLen = 20;
constexpr int kSrtpHmacSha1_80TagLen = 10;
constexpr int kSrtpAesGcmTagLen = 16;
constexpr int kSrtpKeyAndSaltLen = kSrtpAes128KeyLen + kSrtpSaltLen;

enum class SrtpEncryptionAlgorithm : std::uint8_t
{
    aesCm = 1,
    aesGcm = 6,
};

enum class SrtpAuthenticationAlgorithm : std::uint8_t
{
    none = 0,
    hmacSha1 = 1,
};

struct SrtpCryptoPolicy
{
    SrtpEncryptionAlgorithm encryptionAlgorithm = SrtpEncryptionAlgorithm::aesCm;
    int encryptionKeyLength = kSrtpAes128KeyLen;
    SrtpAuthenticationAlgorithm authenticationAlgorithm = SrtpAuthenticationAlgorithm::hmacSha1;
    int authenticationKeyLength = kSrtpHmacSha1KeyLen;
    int authenticationTagLength = kSrtpHmacSha1_80TagLen;
};

NX_VMS_COMMON_API int srtpKeyAndSaltLength(const SrtpCryptoPolicy& policy);

struct SrtpCryptoContext
{
    SrtpCryptoPolicy policy;
    std::vector<std::uint8_t> keyAndSalt = std::vector<std::uint8_t>(kSrtpKeyAndSaltLen);
    std::vector<std::uint8_t> mki; //< Master Key Identifier.
    std::optional<std::uint32_t> ssrc;
    std::uint32_t roc = 0;
};

struct EncryptionData
{
    SrtpCryptoContext client;
    SrtpCryptoContext server;
};

struct SrtpSessionDeleter
{
    void operator()(srtp_t session) const noexcept;
};

using SrtpSessionPtr = std::unique_ptr<std::remove_pointer_t<srtp_t>, SrtpSessionDeleter>;

class NX_VMS_COMMON_API SrtpEncryptor
{
public:
    SrtpEncryptor();
    ~SrtpEncryptor();

    bool init(const SrtpCryptoContext& context);

    bool encryptPacket(nx::utils::ByteArray* data, int offset);
    const std::optional<std::uint32_t>& ssrc() const { return m_context.ssrc; }

private:
    bool encryptPacket(uint8_t* data, int* inOutSize);

private:
    SrtpCryptoContext m_context;
    SrtpSessionPtr m_srtp;
};

class NX_VMS_COMMON_API SrtpDecryptor
{
public:
    enum class Result
    {
        success,
        packetRejected,
        error,
    };

    SrtpDecryptor();
    ~SrtpDecryptor();

    bool init(const SrtpCryptoContext& context);

    Result decryptPacket(uint8_t* data, int* inOutSize);

private:
    bool initializeRoc(std::uint32_t ssrc);

private:
    SrtpCryptoContext m_context;
    SrtpSessionPtr m_srtp;
    std::unordered_set<std::uint32_t> m_rocInitializedSsrcs;
};

} // namespace nx::rtsp
