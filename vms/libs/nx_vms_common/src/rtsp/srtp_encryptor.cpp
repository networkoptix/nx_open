// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "srtp_encryptor.h"

#include <QtCore/QtEndian>

#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

using namespace nx::rtp;

namespace nx::vms::server::rtsp
{

class SrtpInit
{
public:
    SrtpInit()
    {
        srtp_init();
    }
    ~SrtpInit()
    {
        srtp_shutdown();
    }
};
static SrtpInit init;

SrtpEncryptor::SrtpEncryptor()
{
}

SrtpEncryptor::~SrtpEncryptor()
{
    srtp_dealloc(m_srtpIn);
    srtp_dealloc(m_srtpOut);
}

bool SrtpEncryptor::init(const EncryptionData& masterKeys)
{
    m_masterKeys = masterKeys;

    if (srtp_err_status_t err = srtp_create(&m_srtpIn, nullptr))
    {
        NX_WARNING(this, "Can't create SRTP session. Error: %1", static_cast<int>(err));
        return false;
    }
    if (srtp_err_status_t err = srtp_create(&m_srtpOut, nullptr))
    {
        srtp_dealloc(m_srtpIn);
        NX_WARNING(this, "Can't create SRTP session. Error: %1", static_cast<int>(err));
        return false;
    }

    srtp_policy_t inbound = {};
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&inbound.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&inbound.rtcp);
    inbound.ssrc.type = ssrc_any_inbound;
    // SRTP lib uses key+salt as a single buffer. They are arranged in the struct at such order.
    inbound.key = m_isClient ? m_masterKeys.serverKeyAndSalt : m_masterKeys.clientKeyAndSalt;
    inbound.window_size = 1024;
    inbound.allow_repeat_tx = true;
    inbound.next = nullptr;

    if (srtp_err_status_t err = srtp_add_stream(m_srtpIn, &inbound))
    {
        NX_WARNING(this, "SRTP add inbound stream failed, status=%1", (int)err);
        return false;
    }

    srtp_policy_t outbound = {};
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&outbound.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&outbound.rtcp);
    outbound.ssrc.type = ssrc_any_outbound;
    // SRTP lib uses key+salt as a single buffer. They are arranged in the struct at such order.
    outbound.key = m_isClient ? m_masterKeys.clientKeyAndSalt : m_masterKeys.serverKeyAndSalt;
    outbound.window_size = 1024;
    outbound.allow_repeat_tx = true;
    outbound.next = nullptr;

    if (srtp_err_status_t err = srtp_add_stream(m_srtpOut, &outbound))
    {
        NX_WARNING(this, "SRTP add outbound stream failed, status=%1", (int)err);
        return false;
    }
    return true;
}

bool SrtpEncryptor::encryptPacket(nx::utils::ByteArray* data, int offset)
{
    if (data->size() % 4)
    {
        nx::rtp::RtpHeader* packet =
            (nx::rtp::RtpHeader*)(data->data() + offset);
        if (!packet->isRtcp())
        {
            uint8_t padding = 4 - data->size() % 4;
            packet->padding = true;
            if (padding > 1)
                data->write("\x00\x00\x00", padding - 1);
            data->write((const char*)&padding, 1);
        }
    }

    data->reserve(data->size() + SRTP_MAX_TRAILER_LEN);
    int size = data->size() - offset;
    bool result = encryptPacket((uint8_t*)data->data() + offset, &size);
    data->resize(size + offset);
    return result;
}

bool SrtpEncryptor::encryptPacket(uint8_t* data, int* inOutSize)
{
    nx::rtp::RtpHeader* packet = (nx::rtp::RtpHeader*)(data);
    srtp_err_status_t error = packet->isRtcp()
        ? srtp_protect_rtcp(m_srtpOut, data, inOutSize)
        : srtp_protect(m_srtpOut, data, inOutSize);
    if (error == srtp_err_status_replay_fail)
        NX_WARNING(this, "Outgoing SRTCP packet is a replay");
    else if (error)
        NX_WARNING(this, "SRTCP protect error, status=%1", (int) error);
    NX_VERBOSE(this, "Protected SRTCP packet size=%1", *inOutSize);

    return error == 0;
}

bool SrtpEncryptor::decryptPacket(uint8_t* data, int* inOutSize)
{
    nx::rtp::RtpHeader* packet = (nx::rtp::RtpHeader*)(data);
    srtp_err_status_t error = packet->isRtcp()
        ? srtp_unprotect_rtcp(m_srtpIn, data, inOutSize)
        : srtp_unprotect(m_srtpIn, data, inOutSize);
    if (error == srtp_err_status_replay_fail)
        NX_WARNING(this, "Incoming SRTCP packet is a replay");
    else if (error)
        NX_WARNING(this, "SRTCP unprotect error, status=%1", (int)error);
    NX_VERBOSE(this, "Unprotected SRTCP packet size=%1", *inOutSize);

    return error == 0;
}

} // namespace nx::vms::server::rtsp
