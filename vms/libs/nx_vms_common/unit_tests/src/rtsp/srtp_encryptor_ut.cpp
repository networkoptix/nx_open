// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <numeric>

#include <gtest/gtest.h>

#include <QtCore/QByteArray>

#include <nx/rtp/rtcp.h>
#include <rtsp/srtp_encryptor.h>

namespace nx::rtsp::test {
namespace {

constexpr SrtpCryptoPolicy kAes128CmPolicy{};
constexpr SrtpCryptoPolicy kAes256CmPolicy{
    .encryptionAlgorithm = SrtpEncryptionAlgorithm::aesCm,
    .encryptionKeyLength = kSrtpAes256KeyLen,
    .authenticationAlgorithm = SrtpAuthenticationAlgorithm::hmacSha1,
    .authenticationKeyLength = kSrtpHmacSha1KeyLen,
    .authenticationTagLength = kSrtpHmacSha1_80TagLen,
};
constexpr SrtpCryptoPolicy kAes128GcmPolicy{
    .encryptionAlgorithm = SrtpEncryptionAlgorithm::aesGcm,
    .encryptionKeyLength = kSrtpAes128KeyLen,
    .authenticationAlgorithm = SrtpAuthenticationAlgorithm::none,
    .authenticationKeyLength = 0,
    .authenticationTagLength = kSrtpAesGcmTagLen,
};
constexpr SrtpCryptoPolicy kAes256GcmPolicy{
    .encryptionAlgorithm = SrtpEncryptionAlgorithm::aesGcm,
    .encryptionKeyLength = kSrtpAes256KeyLen,
    .authenticationAlgorithm = SrtpAuthenticationAlgorithm::none,
    .authenticationKeyLength = 0,
    .authenticationTagLength = kSrtpAesGcmTagLen,
};

void assertRoundTrip(const SrtpCryptoPolicy& policy,
    const std::vector<std::uint8_t>& mki,
    bool moveMkiAfterAuthTag = false)
{
    EncryptionData encryptionData;
    encryptionData.server.policy = policy;
    const bool isGcm = policy.encryptionAlgorithm == SrtpEncryptionAlgorithm::aesGcm;
    const int keyAndSaltLength = srtpKeyAndSaltLength(policy);
    ASSERT_GT(keyAndSaltLength, 0);
    encryptionData.server.keyAndSalt.resize(keyAndSaltLength);
    std::iota(encryptionData.server.keyAndSalt.begin(), encryptionData.server.keyAndSalt.end(), 1);
    encryptionData.server.mki = mki;

    SrtpEncryptor server;
    ASSERT_TRUE(server.init(encryptionData.server));
    SrtpDecryptor client;
    ASSERT_TRUE(client.init(encryptionData.server));

    const QByteArray plainPacket = QByteArray::fromHex("80600001000000011234567801020304");
    nx::utils::ByteArray packet;
    packet.write(plainPacket);

    ASSERT_TRUE(server.encryptPacket(&packet, 0));
    if (!mki.empty())
    {
        const int mkiOffset = (int) packet.size() - (int) mki.size() - (isGcm ? 0 : 10);
        ASSERT_GE(mkiOffset, 0);
        EXPECT_TRUE(std::equal(mki.cbegin(),
            mki.cend(),
            reinterpret_cast<const std::uint8_t*>(packet.data()) + mkiOffset));

        if (moveMkiAfterAuthTag && !isGcm)
        {
            constexpr int kAuthTagSize = 10;
            memmove(
                packet.data() + mkiOffset, packet.data() + mkiOffset + mki.size(), kAuthTagSize);
            memcpy(packet.data() + packet.size() - mki.size(), mki.data(), mki.size());
        }
    }

    int packetSize = (int) packet.size();
    ASSERT_EQ(SrtpDecryptor::Result::success,
        client.decryptPacket(reinterpret_cast<std::uint8_t*>(packet.data()), &packetSize));

    EXPECT_EQ(QByteArray(packet.data(), packetSize), plainPacket);
}

TEST(SrtpEncryptor, aes256WithoutMki)
{
    assertRoundTrip(kAes256CmPolicy, {});
}

TEST(SrtpEncryptor, aes128WithMki)
{
    assertRoundTrip(kAes128CmPolicy, {0, 0, 0, 1});
}

TEST(SrtpEncryptor, aes128WithTrailingMki)
{
    assertRoundTrip(kAes128CmPolicy, {0, 0, 0, 1}, true);
}

TEST(SrtpEncryptor, aes128GcmWithMki)
{
    assertRoundTrip(kAes128GcmPolicy, {0, 0, 0, 1});
}

TEST(SrtpEncryptor, aes256GcmWithoutMki)
{
    assertRoundTrip(kAes256GcmPolicy, {});
}

TEST(SrtpEncryptor, acceptsSsrcDifferentFromMikey)
{
    SrtpCryptoContext context;
    std::iota(context.keyAndSalt.begin(), context.keyAndSalt.end(), 1);
    context.ssrc = 0x87654321;

    SrtpEncryptor server;
    ASSERT_TRUE(server.init(context));
    SrtpDecryptor client;
    ASSERT_TRUE(client.init(context));

    for (const char* packetHex:
        {"80600001000000011234567801020304", "80600001000000021234567901020304"})
    {
        const QByteArray plainPacket = QByteArray::fromHex(packetHex);
        nx::utils::ByteArray packet;
        packet.write(plainPacket);
        ASSERT_TRUE(server.encryptPacket(&packet, 0));

        int packetSize = (int) packet.size();
        ASSERT_EQ(SrtpDecryptor::Result::success,
            client.decryptPacket(reinterpret_cast<std::uint8_t*>(packet.data()), &packetSize));
        EXPECT_EQ(QByteArray(packet.data(), packetSize), plainPacket);
    }
}

TEST(SrtpEncryptor, appliesRocToActualSsrc)
{
    SrtpCryptoContext serverContext;
    std::iota(serverContext.keyAndSalt.begin(), serverContext.keyAndSalt.end(), 1);

    SrtpEncryptor server;
    ASSERT_TRUE(server.init(serverContext));

    nx::utils::ByteArray lastPacket;
    for (const char* packetHex:
        {"8060ffff000000011234567801020304", "80600000000000021234567801020304"})
    {
        lastPacket.clear();
        lastPacket.write(QByteArray::fromHex(packetHex));
        ASSERT_TRUE(server.encryptPacket(&lastPacket, 0));
    }

    SrtpCryptoContext clientContext = serverContext;
    clientContext.ssrc = 0x87654321; //< Different from the actual SSRC.
    clientContext.roc = 1;
    SrtpDecryptor client;
    ASSERT_TRUE(client.init(clientContext));

    int packetSize = (int) lastPacket.size();
    ASSERT_EQ(SrtpDecryptor::Result::success,
        client.decryptPacket(reinterpret_cast<std::uint8_t*>(lastPacket.data()), &packetSize));
    EXPECT_EQ(QByteArray(lastPacket.data(), packetSize),
        QByteArray::fromHex("80600000000000021234567801020304"));
}

TEST(SrtpEncryptor, reportsReplayedPacket)
{
    SrtpCryptoContext context;
    std::iota(context.keyAndSalt.begin(), context.keyAndSalt.end(), 1);

    SrtpEncryptor server;
    ASSERT_TRUE(server.init(context));
    SrtpDecryptor client;
    ASSERT_TRUE(client.init(context));

    nx::utils::ByteArray packet;
    packet.write(QByteArray::fromHex("80600001000000011234567801020304"));
    ASSERT_TRUE(server.encryptPacket(&packet, 0));
    QByteArray duplicate(packet.data(), packet.size());

    int packetSize = (int) packet.size();
    ASSERT_EQ(SrtpDecryptor::Result::success,
        client.decryptPacket(reinterpret_cast<std::uint8_t*>(packet.data()), &packetSize));

    int duplicateSize = duplicate.size();
    EXPECT_EQ(SrtpDecryptor::Result::packetRejected,
        client.decryptPacket(reinterpret_cast<std::uint8_t*>(duplicate.data()), &duplicateSize));
}

void assertSrtcpRoundTrip(const SrtpCryptoPolicy& policy)
{
    constexpr std::uint32_t kSsrc = 0x12345678;
    SrtpCryptoContext context;
    context.policy = policy;
    context.keyAndSalt.resize(srtpKeyAndSaltLength(policy));
    std::iota(context.keyAndSalt.begin(), context.keyAndSalt.end(), 1);
    context.mki = {0, 0, 0, 1};
    context.ssrc = kSsrc;

    SrtpEncryptor server;
    ASSERT_TRUE(server.init(context));
    SrtpDecryptor client;
    ASSERT_TRUE(client.init(context));

    nx::rtp::RtcpSenderReport report;
    report.ssrc = kSsrc;
    report.ntpTimestamp = 1'552'500'923'000'000;
    std::array<std::uint8_t, nx::rtp::RtcpSenderReport::kSize> plainPacket;
    ASSERT_EQ(
        report.write(plainPacket.data(), (int) plainPacket.size()), (int) plainPacket.size());

    nx::utils::ByteArray packet;
    packet.write(reinterpret_cast<const char*>(plainPacket.data()), plainPacket.size());
    ASSERT_TRUE(server.encryptPacket(&packet, 0));

    int packetSize = (int) packet.size();
    ASSERT_EQ(SrtpDecryptor::Result::success,
        client.decryptPacket(reinterpret_cast<std::uint8_t*>(packet.data()), &packetSize));
    EXPECT_EQ(QByteArray(packet.data(), packetSize),
        QByteArray(reinterpret_cast<const char*>(plainPacket.data()), plainPacket.size()));
}

TEST(SrtpEncryptor, srtcpAes128CmWithMkiRoundTrip)
{
    assertSrtcpRoundTrip(kAes128CmPolicy);
}

TEST(SrtpEncryptor, srtcpAes128GcmWithMkiRoundTrip)
{
    assertSrtcpRoundTrip(kAes128GcmPolicy);
}

TEST(SrtpEncryptor, rejectsIncompatibleEncryptionAndAuthenticationAlgorithms)
{
    SrtpCryptoContext context;
    context.policy = kAes128GcmPolicy;
    context.policy.authenticationAlgorithm = SrtpAuthenticationAlgorithm::hmacSha1;
    context.policy.authenticationKeyLength = kSrtpHmacSha1KeyLen;
    context.keyAndSalt.resize(kSrtpAes128KeyLen + kSrtpAeadSaltLen);

    SrtpEncryptor encryptor;
    EXPECT_FALSE(encryptor.init(context));
}

} // namespace
} // namespace nx::rtsp::test
