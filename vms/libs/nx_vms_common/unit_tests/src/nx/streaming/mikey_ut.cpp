// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QByteArray>

#include <nx/streaming/mikey.h>
#include <nx/utils/base64.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/std_string_utils.h>

namespace nx::streaming::rtsp::test {
namespace {

constexpr std::string_view kAes256CmMikey =
    "AQAFAEU/VsoBAAB2G73bAAAAAAsA7hyBwQEupS4KELHQy5v2FHdFI/iTwbm6whMBAAAAFQABAQEB"
    "IAIBAQMBCgcBAQgBAQoBAQAAADIAIAAuw6E7jJyyHO6EDhc9WDP3D2ct3PB1bIU433BkLYCw03WH"
    "KO639QYLmqb2hHUjCwA=";

constexpr std::string_view kAxisMikey =
    "AQAFAJy5yWQBAABpbxJIAAAAAAsA7h7wcVt3bEgKEBTiXX9BnS8aH1LdCb0NcUEBAAAAFQABAQEB"
    "EAIBAQMBCgcBAQgBAQoBAQAAACIAIAAeO741gC1/tzaOFlwe89iZTkduk6SfjkL+dWJJDI8aAA==";

constexpr nx::rtsp::SrtpCryptoPolicy kAes128GcmPolicy{
    .encryptionAlgorithm = nx::rtsp::SrtpEncryptionAlgorithm::aesGcm,
    .encryptionKeyLength = nx::rtsp::kSrtpAes128KeyLen,
    .authenticationAlgorithm = nx::rtsp::SrtpAuthenticationAlgorithm::none,
    .authenticationKeyLength = 0,
    .authenticationTagLength = nx::rtsp::kSrtpAesGcmTagLen,
};

constexpr std::uint8_t kAuthenticationTagLengthParam = 11;
constexpr std::uint8_t kAeadAuthenticationTagLengthParam = 20;

struct PolicyParameter
{
    std::uint8_t type;
    std::uint8_t value;
    size_t typeOffset;
    size_t valueOffset;
};

std::optional<std::vector<PolicyParameter>> parseGeneratedPolicyParameters(
    std::string_view decoded)
{
    try
    {
        const auto* data = reinterpret_cast<const std::uint8_t*>(decoded.data());
        nx::utils::BitStreamReader reader(data, data + decoded.size());

        reader.skipBytes(19); //< MIKEY header.
        if (reader.getBits(8) != 11 || reader.getBits(8) != 0) //< RAND, NTP-UTC.
            return std::nullopt;
        reader.skipBytes(8); //< Timestamp.

        if (reader.getBits(8) != 10) //< Security Policy.
            return std::nullopt;
        reader.skipBytes(reader.getBits(8)); //< RAND.

        reader.skipBytes(3); //< Next payload, policy number and protocol type.
        const std::uint16_t parametersLength = reader.getBits(16);
        const size_t parametersOffset = reader.getBitsCount() / 8;
        if (reader.bitsLeft() < parametersLength * 8)
            return std::nullopt;

        std::vector<PolicyParameter> result;
        size_t offset = parametersOffset;
        const size_t parametersEnd = parametersOffset + parametersLength;
        while (offset < parametersEnd)
        {
            if (offset + 2 > parametersEnd)
                return std::nullopt;
            const std::uint8_t type = data[offset];
            const std::uint8_t length = data[offset + 1];
            if (offset + 2 + length > parametersEnd)
                return std::nullopt;
            if (length == 1)
                result.push_back({type, data[offset + 2], offset, offset + 2});
            offset += 2 + length;
        }
        return result;
    }
    catch (const nx::utils::BitStreamException&)
    {
        return std::nullopt;
    }
}

const PolicyParameter* findPolicyParameter(
    const std::vector<PolicyParameter>& parameters, std::uint8_t type)
{
    const auto it = std::find_if(parameters.cbegin(),
        parameters.cend(),
        [type](const PolicyParameter& parameter) { return parameter.type == type; });
    return it == parameters.cend() ? nullptr : &*it;
}

void expectAuthenticationTagParameter(std::string_view base64Payload,
    std::uint8_t expectedType,
    std::uint8_t expectedValue,
    std::uint8_t unexpectedType)
{
    const std::string decoded = nx::utils::fromBase64(base64Payload);
    ASSERT_FALSE(decoded.empty());
    const auto parameters = parseGeneratedPolicyParameters(decoded);
    ASSERT_TRUE(parameters);
    const auto* authenticationTag = findPolicyParameter(*parameters, expectedType);
    ASSERT_NE(authenticationTag, nullptr);
    EXPECT_EQ(authenticationTag->value, expectedValue);
    EXPECT_EQ(findPolicyParameter(*parameters, unexpectedType), nullptr);
}

std::string replaceAuthenticationTagParameter(
    std::string_view base64Payload, std::uint8_t type, std::uint8_t value)
{
    std::string decoded = nx::utils::fromBase64(base64Payload);
    const auto parameters = parseGeneratedPolicyParameters(decoded);
    if (!parameters)
        return {};
    const auto* authenticationTag =
        findPolicyParameter(*parameters, kAeadAuthenticationTagLengthParam);
    if (!authenticationTag)
        return {};
    decoded[authenticationTag->typeOffset] = static_cast<char>(type);
    decoded[authenticationTag->valueOffset] = static_cast<char>(value);
    return nx::utils::toBase64(decoded);
}

void expectPolicy(
    const nx::rtsp::SrtpCryptoPolicy& actual, const nx::rtsp::SrtpCryptoPolicy& expected)
{
    EXPECT_EQ(actual.encryptionAlgorithm, expected.encryptionAlgorithm);
    EXPECT_EQ(actual.encryptionKeyLength, expected.encryptionKeyLength);
    EXPECT_EQ(actual.authenticationAlgorithm, expected.authenticationAlgorithm);
    EXPECT_EQ(actual.authenticationKeyLength, expected.authenticationKeyLength);
    EXPECT_EQ(actual.authenticationTagLength, expected.authenticationTagLength);
}

nx::rtp::Sdp::Media makeMedia(std::string_view mikey)
{
    nx::rtp::Sdp::Media media;
    media.protocol = "RTP/SAVP";
    media.sdpAttributes = {QString::fromStdString("a=key-mgmt:mikey " + std::string(mikey))};
    return media;
}

std::vector<std::uint8_t> fromHex(const char* hex)
{
    const auto bytes = nx::utils::fromHex(hex);
    return std::vector<std::uint8_t>(bytes.cbegin(), bytes.cend());
}

std::string payloadFromKeyMgmt(const QByteArray& header)
{
    static const QByteArray kPrefix = "data=\"";
    const int start = header.indexOf(kPrefix);
    if (start < 0)
        return {};
    const int payloadStart = start + kPrefix.size();
    const int end = header.indexOf('"', payloadStart);
    if (end < 0)
        return {};
    return header.mid(payloadStart, end - payloadStart).toStdString();
}

std::optional<MikeyData> makeStandardMikeyForMedia(
    const nx::rtp::Sdp::Media& media, const std::vector<std::string>& sessionAttributes = {})
{
    const auto payload = getMikeyPayload(media, sessionAttributes);
    if (!payload)
        return std::nullopt;
    return makeStandardMikey(*payload, nx::Url("rtsps://camera/track"));
}

TEST(Mikey, standardModeRejectsInvalidServerPayload)
{
    const std::string payload = "unsupported";
    EXPECT_FALSE(makeStandardMikey(payload, nx::Url("rtsps://camera/track")));
}

TEST(Mikey, parsesAes256CmServerContext)
{
    const auto mikey = makeStandardMikeyForMedia(makeMedia(kAes256CmMikey));
    ASSERT_TRUE(mikey);

    const auto& server = mikey->encryptionData.server;
    EXPECT_EQ(server.policy.encryptionAlgorithm, nx::rtsp::SrtpEncryptionAlgorithm::aesCm);
    EXPECT_EQ(server.policy.encryptionKeyLength, nx::rtsp::kSrtpAes256KeyLen);
    EXPECT_EQ(
        server.policy.authenticationAlgorithm, nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1);
    EXPECT_EQ(server.policy.authenticationKeyLength, nx::rtsp::kSrtpHmacSha1_80TagLen);
    EXPECT_EQ(server.policy.authenticationTagLength, nx::rtsp::kSrtpHmacSha1_80TagLen);
    EXPECT_EQ(server.keyAndSalt,
        fromHex("c3a13b8c9cb21cee840e173d5833f70f672ddcf0756c8538df70642d80b0d375"
                "8728eeb7f5060b9aa6f68475230b"));
    EXPECT_TRUE(server.mki.empty());
    ASSERT_TRUE(server.ssrc);
    EXPECT_EQ(*server.ssrc, 0x761bbddbu);
    EXPECT_EQ(server.roc, 0u);

    EXPECT_EQ(mikey->encryptionData.client.keyAndSalt.size(), nx::rtsp::kSrtpKeyAndSaltLen);
    EXPECT_TRUE(mikey->encryptionData.client.mki.empty());
}

TEST(Mikey, parsesAxisServerContextFromSessionLevelAttribute)
{
    nx::rtp::Sdp::Media media;
    media.protocol = "RTP/SAVP";
    const std::vector<std::string> sessionAttributes = {
        "a=key-mgmt:mikey " + std::string(kAxisMikey)};

    const auto mikey = makeStandardMikeyForMedia(media, sessionAttributes);
    ASSERT_TRUE(mikey);

    const auto& server = mikey->encryptionData.server;
    EXPECT_EQ(server.policy.encryptionAlgorithm, nx::rtsp::SrtpEncryptionAlgorithm::aesCm);
    EXPECT_EQ(server.policy.encryptionKeyLength, nx::rtsp::kSrtpAes128KeyLen);
    EXPECT_EQ(
        server.policy.authenticationAlgorithm, nx::rtsp::SrtpAuthenticationAlgorithm::hmacSha1);
    EXPECT_EQ(server.policy.authenticationKeyLength, nx::rtsp::kSrtpHmacSha1_80TagLen);
    EXPECT_EQ(server.policy.authenticationTagLength, nx::rtsp::kSrtpHmacSha1_80TagLen);
    EXPECT_EQ(server.keyAndSalt,
        fromHex("3bbe35802d7fb7368e165c1ef3d8994e476e93a49f8e42fe7562490c8f1a"));
    EXPECT_TRUE(server.mki.empty());
    ASSERT_TRUE(server.ssrc);
    EXPECT_EQ(*server.ssrc, 0x696f1248u);
}

TEST(Mikey, mediaLevelKeyManagementOverridesSessionLevel)
{
    nx::rtp::Sdp::Media media;
    media.protocol = "RTP/SAVP";
    media.sdpAttributes = {"a=key-mgmt:unsupported value"};
    const std::vector<std::string> sessionAttributes = {
        "a=key-mgmt:mikey " + std::string(kAxisMikey)};

    EXPECT_FALSE(getMikeyPayload(media, sessionAttributes));
}

TEST(Mikey, standardModeGeneratesClientKeyWithoutMki)
{
    const auto mikey = makeStandardMikeyForMedia(makeMedia(kAes256CmMikey));
    ASSERT_TRUE(mikey);

    const std::string generatedPayload = payloadFromKeyMgmt(mikey->keyMgmtHeader);
    ASSERT_FALSE(generatedPayload.empty());
    expectAuthenticationTagParameter(generatedPayload,
        kAuthenticationTagLengthParam,
        nx::rtsp::kSrtpHmacSha1_80TagLen,
        kAeadAuthenticationTagLengthParam);
    const auto parsedGeneratedMikey = makeStandardMikeyForMedia(makeMedia(generatedPayload));
    ASSERT_TRUE(parsedGeneratedMikey);

    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.keyAndSalt,
        mikey->encryptionData.client.keyAndSalt);
    EXPECT_TRUE(parsedGeneratedMikey->encryptionData.server.mki.empty());
    ASSERT_TRUE(mikey->encryptionData.client.ssrc);
    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.ssrc, mikey->encryptionData.client.ssrc);
}

TEST(Mikey, clientManagedModeUsesClientKeyAndFourByteMkiInBothDirections)
{
    const auto mikey = makeClientManagedMikey(nx::Url("rtsps://camera/track"));
    ASSERT_TRUE(mikey);

    EXPECT_EQ(mikey->encryptionData.client.keyAndSalt, mikey->encryptionData.server.keyAndSalt);
    EXPECT_EQ(mikey->encryptionData.client.mki, fromHex("00000001"));
    EXPECT_EQ(mikey->encryptionData.server.mki, fromHex("00000001"));
    ASSERT_TRUE(mikey->encryptionData.client.ssrc);
    EXPECT_FALSE(mikey->encryptionData.server.ssrc);

    const std::string generatedPayload = payloadFromKeyMgmt(mikey->keyMgmtHeader);
    const auto parsedGeneratedMikey = makeStandardMikeyForMedia(makeMedia(generatedPayload));
    ASSERT_TRUE(parsedGeneratedMikey);
    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.policy.authenticationKeyLength,
        nx::rtsp::kSrtpHmacSha1KeyLen);
    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.mki, fromHex("00000001"));
    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.ssrc, mikey->encryptionData.client.ssrc);
}

TEST(Mikey, clientManagedModeSupportsAes128Gcm)
{
    const auto mikey = makeClientManagedMikey(nx::Url("rtsps://camera/track"), kAes128GcmPolicy);
    ASSERT_TRUE(mikey);

    expectPolicy(mikey->encryptionData.client.policy, kAes128GcmPolicy);
    EXPECT_EQ(mikey->encryptionData.client.keyAndSalt.size(),
        nx::rtsp::kSrtpAes128KeyLen + nx::rtsp::kSrtpAeadSaltLen);

    const std::string generatedPayload = payloadFromKeyMgmt(mikey->keyMgmtHeader);
    expectAuthenticationTagParameter(generatedPayload,
        kAeadAuthenticationTagLengthParam,
        nx::rtsp::kSrtpAesGcmTagLen,
        kAuthenticationTagLengthParam);
    const auto parsedGeneratedMikey = makeStandardMikeyForMedia(makeMedia(generatedPayload));
    ASSERT_TRUE(parsedGeneratedMikey);
    expectPolicy(parsedGeneratedMikey->encryptionData.server.policy, kAes128GcmPolicy);
    EXPECT_EQ(parsedGeneratedMikey->encryptionData.server.keyAndSalt,
        mikey->encryptionData.client.keyAndSalt);
}

TEST(Mikey, parsesAeadAuthenticationTagLengthParameter)
{
    const auto mikey = makeClientManagedMikey(nx::Url("rtsps://camera/track"), kAes128GcmPolicy);
    ASSERT_TRUE(mikey);

    const std::string generatedPayload = payloadFromKeyMgmt(mikey->keyMgmtHeader);
    const std::string invalidTagPayload = replaceAuthenticationTagParameter(
        generatedPayload, kAeadAuthenticationTagLengthParam, nx::rtsp::kSrtpAesGcmTagLen - 1);
    ASSERT_FALSE(invalidTagPayload.empty());
    EXPECT_FALSE(makeStandardMikey(invalidTagPayload, nx::Url("rtsps://camera/track")));
}

TEST(Mikey, doesNotUseRegularAuthenticationTagLengthForAead)
{
    const auto mikey = makeClientManagedMikey(nx::Url("rtsps://camera/track"), kAes128GcmPolicy);
    ASSERT_TRUE(mikey);

    const std::string generatedPayload = payloadFromKeyMgmt(mikey->keyMgmtHeader);
    const std::string regularTagPayload = replaceAuthenticationTagParameter(
        generatedPayload, kAuthenticationTagLengthParam, nx::rtsp::kSrtpHmacSha1_80TagLen);
    ASSERT_FALSE(regularTagPayload.empty());

    const auto parsedMikey = makeStandardMikey(regularTagPayload, nx::Url("rtsps://camera/track"));
    ASSERT_TRUE(parsedMikey);
    EXPECT_EQ(parsedMikey->encryptionData.server.policy.authenticationTagLength,
        nx::rtsp::kSrtpAesGcmTagLen);
}

TEST(Mikey, clientManagedModeUsesProvidedServerPolicy)
{
    const auto gcmMikey =
        makeClientManagedMikey(nx::Url("rtsps://camera/track"), kAes128GcmPolicy);
    ASSERT_TRUE(gcmMikey);

    const auto standardMikey = makeStandardMikey(
        payloadFromKeyMgmt(gcmMikey->keyMgmtHeader), nx::Url("rtsps://camera/track"));
    ASSERT_TRUE(standardMikey);

    const auto mikey = makeClientManagedMikey(
        nx::Url("rtsps://camera/track"), standardMikey->encryptionData.server.policy);
    ASSERT_TRUE(mikey);
    expectPolicy(mikey->encryptionData.client.policy, kAes128GcmPolicy);
    EXPECT_EQ(mikey->encryptionData.client.keyAndSalt.size(),
        nx::rtsp::kSrtpAes128KeyLen + nx::rtsp::kSrtpAeadSaltLen);
}

} // namespace
} // namespace nx::streaming::rtsp::test
