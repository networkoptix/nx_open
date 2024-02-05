// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>
#include <random>

#include <gtest/gtest.h>

#include <nx/network/websocket/websocket_parser.h>
#include <nx/network/websocket/websocket_serializer.h>
#include <nx/utils/base64.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/random.h>

using namespace nx::network::websocket;

TEST(Websockets, DeflateCompressDecompressWithContext)
{
    Serializer serializer(false);
    nx::Buffer outputBuffer;
    Parser parser(Role::client,
        [&](FrameType type, const nx::Buffer& buffer, bool fin)
        {
            outputBuffer = buffer;
            ASSERT_TRUE(fin);
            ASSERT_EQ(type, FrameType::binary);
        });
    auto test =
        [&](size_t size)
        {
            auto buf = nx::utils::random::generate(size);
            auto message = serializer.prepareMessage(
                buf, FrameType::binary, CompressionType::perMessageDeflate);
            parser.consume(message);
            ASSERT_EQ(outputBuffer.size(), buf.size());
            nx::utils::QnCryptographicHash hash1(nx::utils::QnCryptographicHash::Algorithm::Sha256);
            nx::utils::QnCryptographicHash hash2(nx::utils::QnCryptographicHash::Algorithm::Sha256);
            hash1.addData(outputBuffer);
            hash2.addData(buf);
            auto fingerprint1 = nx::utils::toBase64(hash1.result().toStdString());
            auto fingerprint2 = nx::utils::toBase64(hash2.result().toStdString());
            ASSERT_EQ(fingerprint1, fingerprint2);
            outputBuffer.clear();
        };
    for (size_t size = 16; size < 1024 * 1024 * 16; size = size * 2)
    {
        test(size);
    }
}
