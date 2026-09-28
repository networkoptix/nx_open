// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "srtp_encryptor.h"

#include <cstdint>
#include <cstring>

#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

using namespace nx::rtp;

namespace nx::rtsp {

namespace {

bool mkiFollowsAuthTag(SrtpEncryptionAlgorithm encryptionAlgorithm)
{
    return encryptionAlgorithm == SrtpEncryptionAlgorithm::aesGcm;
}

SrtpDecryptor::Result decryptResult(srtp_err_status_t error)
{
    switch (error)
    {
        case srtp_err_status_ok:
            return SrtpDecryptor::Result::success;

        case srtp_err_status_bad_param:
        case srtp_err_status_auth_fail:
        case srtp_err_status_replay_fail:
        case srtp_err_status_replay_old:
        case srtp_err_status_bad_mki:
        case srtp_err_status_pkt_idx_old:
            return SrtpDecryptor::Result::packetRejected;

        default:
            return SrtpDecryptor::Result::error;
    }
}

bool setCryptoPolicy(const SrtpCryptoPolicy& source, srtp_crypto_policy_t* target)
{
    if (source.encryptionAlgorithm == SrtpEncryptionAlgorithm::aesCm)
    {
        // Some MIKEY implementations put the 10-byte tag length into the authentication key
        // length field. The resulting SRTP policy is still HMAC-SHA1 with a 20-byte key.
        if (source.authenticationAlgorithm != SrtpAuthenticationAlgorithm::hmacSha1
            || (source.authenticationKeyLength != kSrtpHmacSha1KeyLen
                && source.authenticationKeyLength != kSrtpHmacSha1_80TagLen)
            || source.authenticationTagLength != kSrtpHmacSha1_80TagLen)
        {
            return false;
        }

        if (source.encryptionKeyLength == kSrtpAes128KeyLen)
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(target);
        else if (source.encryptionKeyLength == kSrtpAes256KeyLen)
            srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(target);
        else
            return false;

        return true;
    }

    if (source.encryptionAlgorithm == SrtpEncryptionAlgorithm::aesGcm)
    {
        if (source.authenticationAlgorithm != SrtpAuthenticationAlgorithm::none
            || source.authenticationKeyLength != 0
            || source.authenticationTagLength != kSrtpAesGcmTagLen)
        {
            return false;
        }

        if (source.encryptionKeyLength == kSrtpAes128KeyLen)
            srtp_crypto_policy_set_aes_gcm_128_16_auth(target);
        else if (source.encryptionKeyLength == kSrtpAes256KeyLen)
            srtp_crypto_policy_set_aes_gcm_256_16_auth(target);
        else
            return false;

        return true;
    }

    return false;
}

} // namespace

int srtpKeyAndSaltLength(const SrtpCryptoPolicy& policy)
{
    srtp_crypto_policy_t target = {};
    return setCryptoPolicy(policy, &target) ? target.cipher_key_len : 0;
}

void SrtpSessionDeleter::operator()(srtp_t session) const noexcept
{
    if (session)
        srtp_dealloc(session);
}

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

SrtpEncryptor::~SrtpEncryptor() = default;

SrtpDecryptor::SrtpDecryptor()
{
}

SrtpDecryptor::~SrtpDecryptor() = default;

namespace {

bool addSrtpStream(srtp_t session,
    const SrtpCryptoContext& context,
    srtp_ssrc_type_t ssrcType,
    std::uint32_t ssrc = 0)
{
    srtp_policy_t policy = {};
    if (!setCryptoPolicy(context.policy, &policy.rtp)
        || !setCryptoPolicy(context.policy, &policy.rtcp)
        || context.keyAndSalt.size() != (size_t) policy.rtp.cipher_key_len)
    {
        return false;
    }

    policy.ssrc.type = ssrcType;
    policy.ssrc.value = ssrc;
    // SRTP lib uses key+salt as a single buffer. They are arranged in the struct at such order.
    policy.key = const_cast<std::uint8_t*>(context.keyAndSalt.data());
    policy.window_size = 1024;
    policy.allow_repeat_tx = true;
    policy.next = nullptr;

    return srtp_add_stream(session, &policy) == srtp_err_status_ok;
}

SrtpSessionPtr createSrtpSession(const SrtpCryptoContext& context, srtp_ssrc_type_t ssrcType)
{
    srtp_t session = nullptr;
    if (srtp_create(&session, nullptr) != srtp_err_status_ok)
        return {};

    SrtpSessionPtr result(session);
    if (!addSrtpStream(result.get(), context, ssrcType))
        return {};

    return result;
}

} // namespace

bool SrtpEncryptor::init(const SrtpCryptoContext& context)
{
    m_context = context;
    m_srtp = createSrtpSession(m_context, ssrc_any_outbound);
    if (!m_srtp)
        NX_WARNING(this, "Failed to initialize SRTP encryptor");
    return m_srtp != nullptr;
}

bool SrtpDecryptor::init(const SrtpCryptoContext& context)
{
    m_context = context;
    m_rocInitializedSsrcs.clear();
    m_srtp = createSrtpSession(m_context, ssrc_any_inbound);
    if (!m_srtp)
        NX_WARNING(this, "Failed to initialize SRTP decryptor");
    return m_srtp != nullptr;
}

bool SrtpDecryptor::initializeRoc(std::uint32_t ssrc)
{
    if (m_context.roc == 0 || m_rocInitializedSsrcs.contains(ssrc))
        return true;

    if (!addSrtpStream(m_srtp.get(), m_context, ssrc_specific, ssrc))
        return false;

    if (srtp_set_stream_roc(m_srtp.get(), ssrc, m_context.roc) != srtp_err_status_ok)
    {
        srtp_remove_stream(m_srtp.get(), ssrc);
        return false;
    }

    m_rocInitializedSsrcs.insert(ssrc);
    return true;
}

bool SrtpEncryptor::encryptPacket(nx::utils::ByteArray* data, int offset)
{
    if (data->size() % 4)
    {
        nx::rtp::RtpHeader* packet = (nx::rtp::RtpHeader*) (data->data() + offset);
        if (!packet->isRtcp())
        {
            uint8_t padding = 4 - data->size() % 4;
            packet->padding = true;
            if (padding > 1)
                data->write("\x00\x00\x00", padding - 1);
            data->write((const char*)&padding, 1);
        }
    }

    data->reserve(data->size() + SRTP_MAX_TRAILER_LEN + m_context.mki.size());
    int size = data->size() - offset;
    bool result = encryptPacket((uint8_t*)data->data() + offset, &size);
    data->resize(size + offset);
    return result;
}

bool SrtpEncryptor::encryptPacket(uint8_t* data, int* inOutSize)
{
    nx::rtp::RtpHeader* packet = (nx::rtp::RtpHeader*)(data);
    srtp_err_status_t error = packet->isRtcp() ? srtp_protect_rtcp(m_srtp.get(), data, inOutSize)
                                               : srtp_protect(m_srtp.get(), data, inOutSize);
    if (error == srtp_err_status_replay_fail)
        NX_WARNING(this, "Outgoing SRTP/SRTCP packet is a replay");
    else if (error)
        NX_WARNING(this, "SRTP/SRTCP protect error, status=%1", (int) error);

    if (!error && !m_context.mki.empty())
    {
        const int mkiSize = (int) m_context.mki.size();
        const int tagSize = m_context.policy.authenticationTagLength;
        const bool trailingMki = mkiFollowsAuthTag(m_context.policy.encryptionAlgorithm);
        const int mkiOffset = trailingMki ? *inOutSize : *inOutSize - tagSize;
        if (!trailingMki)
        {
            memmove( //< Buffers can overlap.
                data + mkiOffset + mkiSize,
                data + mkiOffset,
                tagSize);
        }
        memcpy(data + mkiOffset, m_context.mki.data(), mkiSize);
        *inOutSize += mkiSize;
    }

    NX_VERBOSE(this, "Protected SRTP/SRTCP packet size=%1", *inOutSize);

    return error == 0;
}

SrtpDecryptor::Result SrtpDecryptor::decryptPacket(uint8_t* data, int* inOutSize)
{
    if (!data || !inOutSize || *inOutSize < (int) sizeof(nx::rtp::RtpHeader))
        return Result::packetRejected;

    nx::rtp::RtpHeader* packet = (nx::rtp::RtpHeader*) data;
    const bool isRtcp = packet->isRtcp();
    const int ssrcOffset = isRtcp ? 4 : 8;
    const std::uint32_t ssrc = qFromBigEndian<std::uint32_t>(data + ssrcOffset);
    if (!initializeRoc(ssrc))
    {
        NX_WARNING(this, "Failed to initialize SRTP ROC for SSRC %1", ssrc);
        return Result::error;
    }

    const int mkiSize = (int) m_context.mki.size();
    const int tagSize = m_context.policy.authenticationTagLength;

    if (mkiSize && *inOutSize <= mkiSize + tagSize)
        return Result::packetRejected;

    if (mkiSize)
    {
        // libsrtp is configured with a single negotiated master key, so remove MKI before
        // passing the packet to libsrtp.
        bool trailingMki = mkiFollowsAuthTag(m_context.policy.encryptionAlgorithm);
        int mkiOffset = trailingMki ? *inOutSize - mkiSize : *inOutSize - tagSize - mkiSize;
        if (memcmp(data + mkiOffset, m_context.mki.data(), mkiSize) != 0)
        {
            // GStreamer appends MKI after the auth tag even for AES-CM.
            trailingMki = !trailingMki;
            mkiOffset = trailingMki ? *inOutSize - mkiSize : *inOutSize - tagSize - mkiSize;
            if (memcmp(data + mkiOffset, m_context.mki.data(), mkiSize) != 0)
            {
                NX_VERBOSE(this,
                    "Unexpected SRTP MKI: %1",
                    QByteArray(reinterpret_cast<const char*>(data + mkiOffset), mkiSize).toHex());
                return Result::packetRejected;
            }
        }

        if (!trailingMki)
        {
            memmove( //< Buffers can overlap.
                data + mkiOffset,
                data + mkiOffset + mkiSize,
                tagSize);
        }
        *inOutSize -= mkiSize;
    }

    srtp_err_status_t error = isRtcp ? srtp_unprotect_rtcp(m_srtp.get(), data, inOutSize)
                                     : srtp_unprotect(m_srtp.get(), data, inOutSize);

    const auto result = decryptResult(error);
    if (result == SrtpDecryptor::Result::packetRejected)
        NX_VERBOSE(this, "Incoming SRTP/SRTCP packet rejected, status=%1", (int) error);
    else if (result == SrtpDecryptor::Result::error)
        NX_WARNING(this, "SRTP/SRTCP unprotect error, status=%1", (int) error);
    else
        NX_VERBOSE(this, "Unprotected SRTP/SRTCP packet size=%1", *inOutSize);

    return result;
}

} // namespace nx::rtsp
