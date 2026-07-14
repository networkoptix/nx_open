// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mikey.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include <openssl/rand.h>

#include <QtCore/QRandomGenerator>

#include <nx/rtp/rtcp.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/random.h>
#include <nx/utils/std_string_utils.h>

namespace nx::streaming::rtsp {
namespace {

std::vector<uint8_t> randomBytes(int size)
{
    std::vector<uint8_t> result(size);
    if (RAND_bytes(result.data(), size) != 1)
        result.clear();
    return result;
}

std::string makeClientManagedMikeyPayload(const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& mki,
    MikeyPolicyMode policyMode)
{
    constexpr uint8_t kPayloadLast = 0;
    constexpr uint8_t kPayloadKemac = 1;
    constexpr uint8_t kPayloadTimestamp = 5;
    constexpr uint8_t kPayloadSecurityPolicy = 10;
    constexpr uint8_t kPayloadRand = 11;

    constexpr uint8_t kPolicy = 0;
    constexpr uint8_t kSrtpProtocol = 0;
    constexpr uint8_t kAesCm128 = 1;
    constexpr uint8_t kHmacSha1 = 1;

    constexpr int kRandSize = 16;
    const std::vector<uint8_t> rand = randomBytes(kRandSize);
    if (rand.size() != kRandSize)
        return {};

    const uint64_t timestamp = nx::rtp::unixTimestampToNtpTimestamp(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()));

    enum PolicyParamType: uint8_t
    {
        kEncryptionAlgorithm = 0,
        kEncryptionKeyLength = 1,
        kAuthenticationAlgorithm = 2,
        kAuthenticationKeyLength = 3,
        kSrtpEncryption = 7,
        kSrtcpEncryption = 8,
        kSrtpAuthentication = 10,
        kAuthenticationTagLength = 11,
    };

    constexpr char kPolicyParamLength = 1;
    constexpr char kPolicyParamEnabled = 1;
    constexpr char kAesCm128KeyLength = 16;
    constexpr char kHmacSha1KeyLength = 20;
    constexpr char kHmacSha180TagLength = 10;

    // Each SRTP security policy parameter is encoded as {type, length, value}.
    static const uint8_t kRfcPolicyParams[] = {
        kEncryptionAlgorithm, kPolicyParamLength, char(kAesCm128),
        kEncryptionKeyLength, kPolicyParamLength, kAesCm128KeyLength,
        kAuthenticationAlgorithm, kPolicyParamLength, char(kHmacSha1),
        kAuthenticationKeyLength, kPolicyParamLength, kHmacSha1KeyLength,
        kSrtpEncryption, kPolicyParamLength, kPolicyParamEnabled,
        kSrtcpEncryption, kPolicyParamLength, kPolicyParamEnabled,
        kSrtpAuthentication, kPolicyParamLength, kPolicyParamEnabled,
        kAuthenticationTagLength, kPolicyParamLength, kHmacSha180TagLength
    };

    // This is not RFC-correct: "80" in "hmac-sha1-80" is the authentication tag length,
    // while SRTP auth key length is 20 bytes. GStreamer puts 10 into SRTP_AUTH_KEY_LEN,
    // so keep this variant as a fallback for compatibility.
    static const uint8_t kGstreamerCompatiblePolicyParams[] = {
        kEncryptionAlgorithm, kPolicyParamLength, char(kAesCm128),
        kEncryptionKeyLength, kPolicyParamLength, kAesCm128KeyLength,
        kAuthenticationAlgorithm, kPolicyParamLength, char(kHmacSha1),
        kAuthenticationKeyLength, kPolicyParamLength, kHmacSha180TagLength,
        kSrtpEncryption, kPolicyParamLength, kPolicyParamEnabled,
        kSrtcpEncryption, kPolicyParamLength, kPolicyParamEnabled,
        kSrtpAuthentication, kPolicyParamLength, kPolicyParamEnabled
    };

    const std::vector<uint8_t> policyParams = policyMode == MikeyPolicyMode::rfc
        ? std::vector<uint8_t>(kRfcPolicyParams, kRfcPolicyParams + sizeof(kRfcPolicyParams))
        : std::vector<uint8_t>(kGstreamerCompatiblePolicyParams,
            kGstreamerCompatiblePolicyParams + sizeof(kGstreamerCompatiblePolicyParams));

    constexpr int kMikeyHeaderSize = 19;
    constexpr int kTimestampPayloadSize = 10;
    constexpr int kRandPayloadHeaderSize = 2;
    constexpr int kSecurityPolicyPayloadHeaderSize = 5;
    constexpr int kKemacPayloadHeaderSize = 4;
    constexpr int kKeyDataSubPayloadHeaderSize = 4;
    const int fixedPrefixSize =
        kMikeyHeaderSize
        + kTimestampPayloadSize
        + kRandPayloadHeaderSize + rand.size()
        + kSecurityPolicyPayloadHeaderSize + policyParams.size() //< Security Policy payload.
        + kKemacPayloadHeaderSize
        + kKeyDataSubPayloadHeaderSize;

    std::vector<uint8_t> result;
    result.resize(fixedPrefixSize);

    nx::utils::BitStreamWriter bitstream(result.data(), result.data() + result.size());

    // Write MIKEY header.
    bitstream.putBits(8, 1); //< MIKEY version.
    bitstream.putBits(8, 0); //< PSK_INIT.
    bitstream.putBits(8, kPayloadTimestamp);
    bitstream.putBits(8, 0); //< Verification disabled, MIKEY-1 PRF.
    bitstream.putBits(32, nx::utils::random::number<uint32_t>()); //< CSB ID.
    bitstream.putBits(8, 1); //< Number of crypto sessions.
    bitstream.putBits(8, 0); //< SRTP CS ID map type.
    bitstream.putBits(8, kPolicy);
    bitstream.putBits(32, nx::utils::random::number<uint32_t>()); //< Client/send SSRC.
    bitstream.putBits(32, 0); //< ROC.

    // Write Timestamp payload.
    bitstream.putBits(8, kPayloadRand);
    bitstream.putBits(8, 0); //< NTP-UTC timestamp.
    bitstream.putBits(32, (uint32_t)(timestamp >> 32));
    bitstream.putBits(32, (uint32_t)(timestamp));

    // Write RAND payload.
    bitstream.putBits(8, kPayloadSecurityPolicy);
    bitstream.putBits(8, rand.size());
    bitstream.putBytes(rand.data(), rand.size());

    // Write Security Policy payload.
    bitstream.putBits(8, kPayloadKemac);
    bitstream.putBits(8, kPolicy);
    bitstream.putBits(8, kSrtpProtocol);
    bitstream.putBits(16, policyParams.size());
    bitstream.putBytes(policyParams.data(), policyParams.size());

    // Write KEMAC payload header and Key Data sub-payload header.
    bitstream.putBits(8, kPayloadLast); //< It is last payload.
    bitstream.putBits(8, 0); //< Unencrypted KEMAC.
    const int keyDataSize = 4 + key.size() + 1 + mki.size();
    bitstream.putBits(16, keyDataSize);
    bitstream.putBits(8, kPayloadLast);
    bitstream.putBits(8, (2 << 4) | 1); //< TEK key, SPI/MKI follows.
    bitstream.putBits(16, key.size());
    bitstream.flushBits();
    result.insert(result.end(), key.data(), key.data() + key.size());
    result.push_back(mki.size());
    result.insert(result.end(), mki.data(), mki.data() + mki.size());
    result.push_back(0); //< No KEMAC MAC.

    return nx::utils::toBase64(std::string_view(reinterpret_cast<const char*>(result.data()),
        result.size()));
}

} // namespace

bool isClientManagedMikeyMedia(const nx::rtp::Sdp::Media& media)
{
    if (nx::utils::stricmp(media.protocol, "rtp/savp") != 0
        && nx::utils::stricmp(media.protocol, "rtp/savpf") != 0)
    {
        return false;
    }

    return !std::any_of(
        media.sdpAttributes.cbegin(),
        media.sdpAttributes.cend(),
        [](const QString& attribute)
        {
            return attribute.startsWith("a=key-mgmt:mikey", Qt::CaseInsensitive);
        });
}

std::optional<ClientManagedMikeyData> makeClientManagedMikey(const nx::rtp::Sdp::Media& media,
    const nx::Url& setupUrl, MikeyPolicyMode policyMode)
{
    if (!isClientManagedMikeyMedia(media))
        return std::nullopt;

    ClientManagedMikeyData result;
    const std::vector<unsigned char> key = randomBytes(nx::rtsp::kSrtpKeyAndSaltLen);
    if (key.size() != nx::rtsp::kSrtpKeyAndSaltLen)
        return std::nullopt;

    const std::vector<unsigned char> mki = {0, 0, 0, 1};

    memcpy(result.encryptionData.clientKeyAndSalt, key.data(), key.size());
    memcpy(result.encryptionData.serverKeyAndSalt, key.data(), key.size());
    result.encryptionData.mki.assign(mki.cbegin(), mki.cend());

    const std::string mikeyPayload = makeClientManagedMikeyPayload(key, mki, policyMode);
    if (mikeyPayload.empty())
        return std::nullopt;
    result.keyMgmtHeader = "prot=mikey;uri=\"" + setupUrl.toString().toUtf8()
        + "\";data=\"" + mikeyPayload + "\"";

    return result;
}

} // namespace nx::streaming::rtsp
