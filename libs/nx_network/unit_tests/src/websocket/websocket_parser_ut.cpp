// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>
#include <algorithm>
#include <iterator>
#include <thread>
#include <cstring>
#include <gtest/gtest.h>
#include <nx/network/websocket/websocket_parser.h>
#include <nx/network/websocket/websocket_serializer.h>

using namespace nx::network::websocket;

namespace {

static nx::Buffer prepareMessage(
    const nx::Buffer& payload,
    int frameCount,
    FrameType type,
    bool masked,
    int mask,
    CompressionType compressionType)
{
    Serializer serializer(masked, mask);
    NX_ASSERT(frameCount > 0);
    auto compression = Serializer::defaultCompression(compressionType, type);
    if (frameCount == 1)
        return serializer.prepareMessage(payload, type, compression);

    int desiredFrameSize = (int) payload.size() / frameCount;
    int remain = (int) payload.size() % frameCount;
    nx::Buffer result;
    for (int i = 0, offset = 0; i < frameCount; ++i)
    {
        int frameSize = desiredFrameSize;
        if (remain)
        {
            --remain;
            ++frameSize;
        }
        nx::Buffer frame(payload.data() + offset, frameSize);
        offset += frameSize;
        FrameType opCode = i > 0 ? FrameType::continuation : type;
        result +=
            serializer.prepareFrame(std::move(frame), opCode, i == frameCount - 1, compression);
    }

    return result;
}

static nx::Buffer fillDummyPayload(std::size_t size)
{
    static const nx::Buffer kPattern = "hello";

    nx::Buffer result;
    while (result.size() < size)
        result += kPattern;

    return result;
}

}

struct ParserTestParams
{
    CompressionType compressionType;
    int payloadSize;

    ParserTestParams(CompressionType compressionType, int payloadSize):
        compressionType(compressionType),
        payloadSize(payloadSize)
    {}
};

class WebsocketParserTest : public ::testing::TestWithParam<ParserTestParams>
{
protected:
    WebsocketParserTest()
    {
        m_defaultPayload = fillDummyPayload(GetParam().payloadSize);
    }

    void testWebsocketParserAndSerializer(
        int readSize, int frameCount, FrameType type, bool masked, unsigned int mask)
    {
        int messageOffset = 0;
        auto message = prepareMessage(
            m_defaultPayload, frameCount, type, masked, mask, GetParam().compressionType);

        std::vector<FrameType> frameTypes;
        nx::Buffer payload;
        bool fin;
        Parser parser(Role::client,
            [&](FrameType frameType, const nx::Buffer& frame, bool fin_)
            {
                frameTypes.push_back(frameType);
                payload += frame;
                fin = fin_;
            });
        for (int i = 0; ; ++i)
        {
            int consumeLen = std::min((int) (message.size() - messageOffset), readSize);
            parser.consume(message.data() + i * readSize, consumeLen);
            messageOffset += consumeLen;
            if ((std::size_t) messageOffset >= message.size())
                break;
        }

        ASSERT_EQ((int) frameTypes.size(), frameCount);
        ASSERT_EQ(frameTypes.front(), type);
        ASSERT_EQ(m_defaultPayload, payload);
        ASSERT_TRUE(fin);
    }

private:
    nx::Buffer m_defaultPayload;
};

TEST_P(WebsocketParserTest, NoMask)
{
    testWebsocketParserAndSerializer(26, 1, FrameType::binary, false, 0);
    testWebsocketParserAndSerializer(13, 2, FrameType::binary, false, 0);
    if (GetParam().payloadSize > 20)
    {
        testWebsocketParserAndSerializer(25, 91, FrameType::binary, false, 0);
        testWebsocketParserAndSerializer(256, 32, FrameType::binary, false, 0);
    }
}

TEST_P(WebsocketParserTest, Mask)
{
    testWebsocketParserAndSerializer(13, 1, FrameType::binary, true, 0xfa121a23);
    testWebsocketParserAndSerializer(10, 5, FrameType::text, true, 0xd903);
    testWebsocketParserAndSerializer(1, 7, FrameType::binary, true, 0xfa121423);
    if (GetParam().payloadSize > 20)
    {
        testWebsocketParserAndSerializer(13, 33, FrameType::binary, true, 0xfa121a23);
        testWebsocketParserAndSerializer(25, 91, FrameType::binary, true, 0xfa121423);
        testWebsocketParserAndSerializer(256, 32, FrameType::binary, true, 0xd903);
    }
}

INSTANTIATE_TEST_SUITE_P(WebsocketParserSerializer_differentCompressionModes,
    WebsocketParserTest,
    ::testing::Values(
        ParserTestParams(CompressionType::none, 20),
        ParserTestParams(CompressionType::perMessageDeflate, 20),
        ParserTestParams(CompressionType::none, 1000),
        ParserTestParams(CompressionType::perMessageDeflate, 1000)));
