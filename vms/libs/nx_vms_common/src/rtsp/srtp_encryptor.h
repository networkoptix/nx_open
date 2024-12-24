// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <nx/utils/byte_array.h>
#include <srtp2/srtp.h>

namespace nx::vms::server::rtsp
{

constexpr int kSrtpAes128KeyLen = 16;
constexpr int kSrtpSaltLen = 14;
constexpr int kSrtpKeyAndSaltLen = kSrtpAes128KeyLen + kSrtpSaltLen;

struct EncryptionData
{
    uint8_t clientKeyAndSalt[kSrtpKeyAndSaltLen];
    uint8_t serverKeyAndSalt[kSrtpKeyAndSaltLen];
};

class NX_VMS_COMMON_API SrtpEncryptor
{
public:
    SrtpEncryptor();
    ~SrtpEncryptor();

    bool init(const EncryptionData& masterKeys);

    bool encryptPacket(nx::utils::ByteArray* data, int offset);

    bool decryptPacket(uint8_t* data, int* inOutSize);
private:
    bool encryptPacket(uint8_t* data, int* inOutSize);
private:
    EncryptionData m_masterKeys;
    srtp_t m_srtpIn = nullptr;
    srtp_t m_srtpOut = nullptr;
    bool m_isClient = false;
};

} // namespace nx::vms::server::rtsp
