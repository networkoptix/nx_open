// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mikey.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/rand.h>

#include <nx/rtp/rtcp.h>
#include <nx/utils/base64.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/random.h>
#include <nx/utils/std_string_utils.h>

namespace nx::streaming::rtsp {
namespace {

constexpr std::uint8_t kPayloadLast = 0;
constexpr std::uint8_t kPayloadKemac = 1;
constexpr std::uint8_t kPayloadTimestamp = 5;
constexpr std::uint8_t kPayloadSecurityPolicy = 10;
constexpr std::uint8_t kPayloadRand = 11;

constexpr std::uint8_t kPolicy = 0;
constexpr std::uint8_t kSrtpProtocol = 0;
constexpr std::string_view kKeyManagementAttributePrefix = "a=key-mgmt:";

std::optional<nx::rtsp::SrtpEncryptionAlgorithm> parseEncryptionAlgorithm(std::uint8_t value)
{
    switch (value)
    {
        case static_cast<std::uint8_t>(nx::rtsp::SrtpEncryptionAlgorithm::aesCm):
            return nx::rtsp::SrtpEncryptionAlgorithm::aesCm;
        case static_cast<std::uint8_t>(nx::rtsp::SrtpEncryptionAlgorithm::aesGcm):
            return nx::rtsp::SrtpEncryptionAlgorithm::aesGcm;
        default:
            return std::nullopt;
    }
}

std::optional<nx::rtsp::SrtpAuthenticationAlgorithm> parseAuthenticationAlgorithm(
    std::uint8_t value)
{
    switch (value)
    {
        case static_cast<std::uint8_t>(nx::rtsp::SrtpAuthenticationAlgorithm::none):
            return nx::rtsp::SrtpAuthenticationAlgorithm::none;
        case static_cast<std::uint8_t>(nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1):
            return nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1;
        default:
            return std::nullopt;
    }
}

enum PolicyParamType : std::uint8_t
{
    kEncryptionAlgorithm = 0,
    kEncryptionKeyLength = 1,
    kAuthenticationAlgorithm = 2,
    kAuthenticationKeyLength = 3,
    kSrtpEncryption = 7,
    kSrtcpEncryption = 8,
    kSrtpAuthentication = 10,
    kAuthenticationTagLength = 11,
    kAeadAuthenticationTagLength = 20,
};

struct SecurityPolicy
{
    std::uint8_t nextPayload = 0;
    std::uint8_t id = 0;
    std::uint8_t protocol = 0;
    std::optional<std::uint8_t> encryptionAlgorithm;
    std::optional<std::uint8_t> encryptionKeyLength;
    std::optional<std::uint8_t> authenticationAlgorithm;
    std::optional<std::uint8_t> authenticationKeyLength;
    std::optional<std::uint8_t> authenticationTagLength;
    std::optional<std::uint8_t> aeadAuthenticationTagLength;
    std::optional<std::uint8_t> srtpEncryption;
    std::optional<std::uint8_t> srtcpEncryption;
    std::optional<std::uint8_t> srtpAuthentication;
};

struct Kemac
{
    std::uint8_t nextPayload = 0;
    std::vector<std::uint8_t> keyAndSalt;
    std::vector<std::uint8_t> mki;
};

std::vector<std::uint8_t> randomBytes(int size)
{
    std::vector<std::uint8_t> result(size);
    if (RAND_bytes(result.data(), size) != 1)
    {
        for (auto& byte: result)
            byte = nx::utils::random::number(0, 255);
    }
    return result;
}

bool isSavpMedia(const nx::rtp::Sdp::Media& media)
{
    return nx::utils::stricmp(media.protocol, "rtp/savp") == 0
        || nx::utils::stricmp(media.protocol, "rtp/savpf") == 0;
}

std::optional<std::string> parseMikeyAttribute(std::string_view attribute)
{
    if (!nx::utils::startsWith(
            attribute, kKeyManagementAttributePrefix, nx::utils::CaseSensitivity::off))
    {
        return std::nullopt;
    }

    const std::string_view value =
        nx::utils::trim(attribute.substr(kKeyManagementAttributePrefix.size()));
    const size_t separator = value.find(' ');
    const std::string_view protocol = value.substr(0, separator);
    if (nx::utils::stricmp(protocol, "mikey") != 0)
        return std::nullopt;

    return separator == std::string_view::npos
        ? std::string()
        : std::string(nx::utils::trim(value.substr(separator + 1)));
}

std::optional<std::string> findMikeyPayload(const std::vector<std::string>& attributes)
{
    for (const std::string& attribute: attributes)
    {
        if (auto payload = parseMikeyAttribute(attribute))
            return payload;
    }

    return std::nullopt;
}

std::optional<std::string> findMikeyPayload(
    const nx::rtp::Sdp::Media& media, const std::vector<std::string>& sessionSdpAttributes)
{
    bool hasMediaKeyManagement = false;
    for (const auto& attribute: media.sdpAttributes)
    {
        const std::string value = attribute.toStdString();
        if (!nx::utils::startsWith(
                value, kKeyManagementAttributePrefix, nx::utils::CaseSensitivity::off))
        {
            continue;
        }

        hasMediaKeyManagement = true;
        if (auto payload = parseMikeyAttribute(value))
            return payload;
    }

    return hasMediaKeyManagement ? std::nullopt : findMikeyPayload(sessionSdpAttributes);
}

std::optional<std::uint8_t> parseTimestamp(nx::utils::BitStreamReader* reader)
{
    const std::uint8_t nextPayload = reader->getBits(8);
    const std::uint8_t timestampType = reader->getBits(8);

    switch (timestampType)
    {
        case 0: //< NTP-UTC.
        case 1: //< NTP.
            reader->skipBytes(8);
            break;
        case 2: //< Counter.
            reader->skipBytes(4);
            break;
        default:
            return std::nullopt;
    }

    return nextPayload;
}

std::optional<std::uint8_t> parseRand(nx::utils::BitStreamReader* reader)
{
    const std::uint8_t nextPayload = reader->getBits(8);
    const std::uint8_t length = reader->getBits(8);
    reader->skipBytes(length);
    return nextPayload;
}

std::optional<SecurityPolicy> parseSecurityPolicy(nx::utils::BitStreamReader* reader)
{
    SecurityPolicy result;
    result.nextPayload = reader->getBits(8);
    result.id = reader->getBits(8);
    result.protocol = reader->getBits(8);
    const std::uint16_t parametersLength = reader->getBits(16);

    std::vector<std::uint8_t> parametersData(parametersLength);
    if (parametersLength == 0)
        return result;
    reader->readData(parametersData.data(), parametersLength);
    nx::utils::BitStreamReader parameters(
        parametersData.data(), parametersData.data() + parametersData.size());

    while (parameters.bitsLeft() > 0)
    {
        if (parameters.bitsLeft() < 16)
            return std::nullopt;

        const std::uint8_t type = parameters.getBits(8);
        const std::uint8_t length = parameters.getBits(8);
        if (parameters.bitsLeft() < length * 8)
            return std::nullopt;

        std::uint8_t value = 0;
        if (length == 1)
            value = parameters.getBits(8);
        else
            parameters.skipBytes(length);

        if (length != 1)
            continue;

        switch (type)
        {
            case kEncryptionAlgorithm:
                result.encryptionAlgorithm = value;
                break;
            case kEncryptionKeyLength:
                result.encryptionKeyLength = value;
                break;
            case kAuthenticationAlgorithm:
                result.authenticationAlgorithm = value;
                break;
            case kAuthenticationKeyLength:
                result.authenticationKeyLength = value;
                break;
            case kSrtpEncryption:
                result.srtpEncryption = value;
                break;
            case kSrtcpEncryption:
                result.srtcpEncryption = value;
                break;
            case kSrtpAuthentication:
                result.srtpAuthentication = value;
                break;
            case kAuthenticationTagLength:
                result.authenticationTagLength = value;
                break;
            case kAeadAuthenticationTagLength:
                result.aeadAuthenticationTagLength = value;
                break;
        }
    }

    return result;
}

std::optional<Kemac> parseKemac(nx::utils::BitStreamReader* reader)
{
    Kemac result;
    result.nextPayload = reader->getBits(8);
    const std::uint8_t encryptionAlgorithm = reader->getBits(8);
    const std::uint16_t encryptedDataLength = reader->getBits(16);
    if (encryptionAlgorithm != 0 || encryptedDataLength == 0)
        return std::nullopt;

    std::vector<std::uint8_t> encryptedDataBytes(encryptedDataLength);
    reader->readData(encryptedDataBytes.data(), encryptedDataLength);
    nx::utils::BitStreamReader encryptedData(
        encryptedDataBytes.data(), encryptedDataBytes.data() + encryptedDataBytes.size());

    const std::uint8_t keyDataNextPayload = encryptedData.getBits(8);
    const std::uint8_t typeAndValidity = encryptedData.getBits(8);
    const std::uint16_t keyDataLength = encryptedData.getBits(16);

    const std::uint8_t keyType = typeAndValidity >> 4;
    const std::uint8_t keyValidity = typeAndValidity & 0x0f;
    if (keyDataNextPayload != kPayloadLast || keyType != 2 //< TEK.
        || encryptedData.bitsLeft() < keyDataLength * 8)
        return std::nullopt;

    result.keyAndSalt.resize(keyDataLength);
    if (keyDataLength > 0)
        encryptedData.readData(result.keyAndSalt.data(), keyDataLength);

    if (keyValidity == 1) //< SPI/MKI.
    {
        const std::uint8_t mkiLength = encryptedData.getBits(8);
        if (encryptedData.bitsLeft() < mkiLength * 8)
            return std::nullopt;

        result.mki.resize(mkiLength);
        if (mkiLength > 0)
            encryptedData.readData(result.mki.data(), mkiLength);
    }
    else if (keyValidity != 0) //< Interval and unknown validity types are not supported.
    {
        return std::nullopt;
    }

    if (encryptedData.bitsLeft() != 0 || reader->getBits(8) != 0) //< No KEMAC MAC.
        return std::nullopt;

    return result;
}

std::optional<nx::rtsp::SrtpCryptoContext> parseMikeyPayload(std::string_view base64Payload)
{
    const std::string decoded = nx::utils::fromBase64(base64Payload);
    if (decoded.empty())
        return std::nullopt;

    try
    {
        const auto* data = reinterpret_cast<const std::uint8_t*>(decoded.data());
        nx::utils::BitStreamReader reader(data, data + decoded.size());

        const std::uint8_t version = reader.getBits(8);
        const std::uint8_t dataType = reader.getBits(8);
        std::uint8_t nextPayload = reader.getBits(8);
        reader.skipBytes(1); //< Verification and PRF function.
        reader.skipBytes(4); //< CSB ID.
        const std::uint8_t cryptoSessionCount = reader.getBits(8);
        const std::uint8_t cryptoSessionMapType = reader.getBits(8);
        if (version != 1 || dataType != 0 //< PSK_INIT.
            || cryptoSessionCount != 1 || cryptoSessionMapType != 0) //< SRTP-ID.
        {
            return std::nullopt;
        }

        const std::uint8_t policy = reader.getBits(8);
        const std::uint32_t ssrc = reader.getBits(32);
        const std::uint32_t roc = reader.getBits(32);

        std::optional<SecurityPolicy> securityPolicy;
        std::vector<std::uint8_t> keyAndSalt;
        std::vector<std::uint8_t> mki;
        for (int payloadCount = 0; nextPayload != kPayloadLast && payloadCount < 16;
            ++payloadCount)
        {
            switch (nextPayload)
            {
                case kPayloadTimestamp:
                {
                    const auto parsedTimestamp = parseTimestamp(&reader);
                    if (!parsedTimestamp)
                        return std::nullopt;
                    nextPayload = *parsedTimestamp;
                    break;
                }
                case kPayloadRand:
                {
                    const auto parsedRand = parseRand(&reader);
                    if (!parsedRand)
                        return std::nullopt;
                    nextPayload = *parsedRand;
                    break;
                }
                case kPayloadSecurityPolicy:
                {
                    auto parsedPolicy = parseSecurityPolicy(&reader);
                    if (!parsedPolicy)
                        return std::nullopt;
                    nextPayload = parsedPolicy->nextPayload;
                    if (parsedPolicy->id == policy)
                    {
                        if (parsedPolicy->protocol != kSrtpProtocol)
                            return std::nullopt;
                        securityPolicy = std::move(*parsedPolicy);
                    }
                    break;
                }
                case kPayloadKemac:
                {
                    auto parsedKemac = parseKemac(&reader);
                    if (!parsedKemac)
                        return std::nullopt;
                    nextPayload = parsedKemac->nextPayload;
                    keyAndSalt = std::move(parsedKemac->keyAndSalt);
                    mki = std::move(parsedKemac->mki);
                    break;
                }
                default:
                    return std::nullopt;
            }
        }

        if (nextPayload != kPayloadLast || !securityPolicy || !securityPolicy->encryptionAlgorithm
            || !securityPolicy->authenticationAlgorithm || !securityPolicy->encryptionKeyLength
            || (securityPolicy->srtpEncryption && securityPolicy->srtpEncryption != 1)
            || (securityPolicy->srtcpEncryption && securityPolicy->srtcpEncryption != 1)
            || (securityPolicy->srtpAuthentication && securityPolicy->srtpAuthentication != 1))
        {
            return std::nullopt;
        }

        const auto encryptionAlgorithm =
            parseEncryptionAlgorithm(*securityPolicy->encryptionAlgorithm);
        const auto authenticationAlgorithm =
            parseAuthenticationAlgorithm(*securityPolicy->authenticationAlgorithm);
        if (!encryptionAlgorithm || !authenticationAlgorithm)
            return std::nullopt;

        nx::rtsp::SrtpCryptoContext result;
        result.policy.encryptionAlgorithm = *encryptionAlgorithm;
        result.policy.encryptionKeyLength = *securityPolicy->encryptionKeyLength;
        result.policy.authenticationAlgorithm = *authenticationAlgorithm;
        result.policy.authenticationKeyLength = securityPolicy->authenticationKeyLength.value_or(
            *authenticationAlgorithm == nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1
                ? nx::rtsp::kSrtpHmacSha1KeyLen
                : 0);
        result.policy.authenticationTagLength =
            *encryptionAlgorithm == nx::rtsp::SrtpEncryptionAlgorithm::aesGcm
            ? securityPolicy->aeadAuthenticationTagLength.value_or(nx::rtsp::kSrtpAesGcmTagLen)
            : securityPolicy->authenticationTagLength.value_or(nx::rtsp::kSrtpHmacSha1_80TagLen);

        const int keyAndSaltLength = nx::rtsp::srtpKeyAndSaltLength(result.policy);
        if (keyAndSaltLength == 0 || keyAndSalt.size() != (size_t) keyAndSaltLength)
            return std::nullopt;

        result.keyAndSalt = std::move(keyAndSalt);
        result.mki = std::move(mki);
        result.ssrc = ssrc;
        result.roc = roc;

        return result;
    }
    catch (const nx::utils::BitStreamException&)
    {
        return std::nullopt;
    }
}

std::string makeMikeyPayload(const std::vector<std::uint8_t>& key,
    const std::vector<std::uint8_t>& mki,
    std::uint32_t ssrc,
    const nx::rtsp::SrtpCryptoPolicy& cryptoPolicy)
{
    constexpr int kRandSize = 16;
    const std::vector<std::uint8_t> rand = randomBytes(kRandSize);

    const std::uint64_t timestamp =
        nx::rtp::unixTimestampToNtpTimestamp(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()));

    constexpr std::uint8_t kPolicyParamLength = 1;
    constexpr std::uint8_t kPolicyParamEnabled = 1;

    const int authenticationKeyLength =
        cryptoPolicy.authenticationAlgorithm == nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1
        ? nx::rtsp::kSrtpHmacSha1KeyLen
        : cryptoPolicy.authenticationKeyLength;

    const std::uint8_t authenticationTagParamType =
        cryptoPolicy.encryptionAlgorithm == nx::rtsp::SrtpEncryptionAlgorithm::aesGcm
        ? kAeadAuthenticationTagLength
        : kAuthenticationTagLength;

    // Each SRTP security policy parameter is encoded as {type, length, value}.
    const std::vector<std::uint8_t> policyParams = {kEncryptionAlgorithm,
        kPolicyParamLength,
        static_cast<std::uint8_t>(cryptoPolicy.encryptionAlgorithm),
        kEncryptionKeyLength,
        kPolicyParamLength,
        static_cast<std::uint8_t>(cryptoPolicy.encryptionKeyLength),
        kAuthenticationAlgorithm,
        kPolicyParamLength,
        static_cast<std::uint8_t>(cryptoPolicy.authenticationAlgorithm),
        kAuthenticationKeyLength,
        kPolicyParamLength,
        static_cast<std::uint8_t>(authenticationKeyLength),
        kSrtpEncryption,
        kPolicyParamLength,
        kPolicyParamEnabled,
        kSrtcpEncryption,
        kPolicyParamLength,
        kPolicyParamEnabled,
        kSrtpAuthentication,
        kPolicyParamLength,
        kPolicyParamEnabled,
        authenticationTagParamType,
        kPolicyParamLength,
        static_cast<std::uint8_t>(cryptoPolicy.authenticationTagLength)};

    constexpr int kMikeyHeaderSize = 19;
    constexpr int kTimestampPayloadSize = 10;
    constexpr int kRandPayloadHeaderSize = 2;
    constexpr int kSecurityPolicyPayloadHeaderSize = 5;
    constexpr int kKemacPayloadHeaderSize = 4;
    constexpr int kKeyDataSubPayloadHeaderSize = 4;
    const int fixedPrefixSize = kMikeyHeaderSize + kTimestampPayloadSize + kRandPayloadHeaderSize
        + (int) rand.size() + kSecurityPolicyPayloadHeaderSize
        + (int) policyParams.size() //< Security Policy payload.
        + kKemacPayloadHeaderSize + kKeyDataSubPayloadHeaderSize;

    std::vector<std::uint8_t> result(fixedPrefixSize);
    nx::utils::BitStreamWriter bitstream(result.data(), result.data() + result.size());

    // Write MIKEY header.
    bitstream.putBits(8, 1); //< MIKEY version.
    bitstream.putBits(8, 0); //< PSK_INIT.
    bitstream.putBits(8, kPayloadTimestamp);
    bitstream.putBits(8, 0); //< Verification disabled, MIKEY-1 PRF.
    bitstream.putBits(32, nx::utils::random::number<std::uint32_t>()); //< CSB ID.
    bitstream.putBits(8, 1); //< Number of crypto sessions.
    bitstream.putBits(8, 0); //< SRTP CS ID map type.
    bitstream.putBits(8, kPolicy);
    bitstream.putBits(32, ssrc); //< Client/send SSRC.
    bitstream.putBits(32, 0); //< ROC.

    // Write Timestamp payload.
    bitstream.putBits(8, kPayloadRand);
    bitstream.putBits(8, 0); //< NTP-UTC timestamp.
    bitstream.putBits(32, (std::uint32_t) (timestamp >> 32));
    bitstream.putBits(32, (std::uint32_t) timestamp);

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
    const int keyDataSize = 4 + (int) key.size() + (mki.empty() ? 0 : 1 + (int) mki.size());
    bitstream.putBits(16, keyDataSize);
    bitstream.putBits(8, kPayloadLast);
    bitstream.putBits(8, (2 << 4) | (mki.empty() ? 0 : 1)); //< TEK and optional SPI/MKI.
    bitstream.putBits(16, key.size());
    bitstream.flushBits();

    result.insert(result.end(), key.cbegin(), key.cend());
    if (!mki.empty())
    {
        result.push_back((std::uint8_t) mki.size());
        result.insert(result.end(), mki.cbegin(), mki.cend());
    }
    result.push_back(0); //< No KEMAC MAC.

    return nx::utils::toBase64(
        std::string_view(reinterpret_cast<const char*>(result.data()), result.size()));
}

std::optional<MikeyData> makeMikeyData(const nx::Url& setupUrl,
    const nx::rtsp::SrtpCryptoPolicy& cryptoPolicy,
    const std::vector<std::uint8_t>& mki)
{
    MikeyData result;
    const int keyAndSaltLength = nx::rtsp::srtpKeyAndSaltLength(cryptoPolicy);
    if (keyAndSaltLength == 0)
        return std::nullopt;

    result.encryptionData.client.policy = cryptoPolicy;
    result.encryptionData.client.keyAndSalt = randomBytes(keyAndSaltLength);
    result.encryptionData.client.mki = mki;
    const auto clientSsrc = nx::utils::random::number<std::uint32_t>();
    result.encryptionData.client.ssrc = clientSsrc;

    const std::string clientMikeyPayload =
        makeMikeyPayload(result.encryptionData.client.keyAndSalt,
            result.encryptionData.client.mki,
            clientSsrc,
            cryptoPolicy);

    result.keyMgmtHeader = "prot=mikey;uri=\"" + setupUrl.toString().toUtf8() + "\";data=\""
        + clientMikeyPayload + "\"";
    return result;
}

} // namespace

std::optional<std::string> getMikeyPayload(
    const nx::rtp::Sdp::Media& media, const std::vector<std::string>& sessionSdpAttributes)
{
    if (!isSavpMedia(media))
        return std::nullopt;

    return findMikeyPayload(media, sessionSdpAttributes);
}

std::optional<MikeyData> makeStandardMikey(
    const std::string& mikeyPayload, const nx::Url& setupUrl)
{
    const auto serverContext = parseMikeyPayload(mikeyPayload);
    if (!serverContext)
        return std::nullopt;

    auto result = makeMikeyData(setupUrl, nx::rtsp::SrtpCryptoPolicy{}, {});
    if (!result)
        return std::nullopt;
    result->encryptionData.server = *serverContext;
    return result;
}

std::optional<MikeyData> makeClientManagedMikey(
    const nx::Url& setupUrl, nx::rtsp::SrtpCryptoPolicy cryptoPolicy)
{
    auto result = makeMikeyData(setupUrl, cryptoPolicy, {0, 0, 0, 1});
    if (!result)
        return std::nullopt;
    result->encryptionData.server = result->encryptionData.client;
    result->encryptionData.server.ssrc.reset(); //< Accept the camera's SSRC.
    return result;
}

} // namespace nx::streaming::rtsp
