#include <vector>
#include <algorithm>
#include <iterator>
#include <thread>
#include <cstring>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/network/websocket/websocket_parser.h>
#include <nx/network/websocket/websocket_serializer.h>

using namespace nx::network::websocket;

namespace {

using ::testing::_;
using ::testing::AtLeast;

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
    if (frameCount == 1)
        return serializer.prepareMessage(payload, type, compressionType);

    nx::Buffer result;
    for (int i = 0; i < frameCount; ++i)
    {
        FrameType opCode = type;
        if (i > 0)
            opCode = FrameType::continuation;

        result += serializer.prepareFrame(payload, opCode, i == frameCount - 1);
    }

    return result;
}

static nx::Buffer fillDummyPayload(int size)
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
    WebsocketParserTest(): m_parser(
        Role::client,
        [this](FrameType frameType, const nx::Buffer& payload, bool fin)
        {
            m_frameTypes.push_back(frameType);
            m_payload = payload;
            m_fin = fin;
        })
    {
        m_defaultPayload = fillDummyPayload(GetParam().payloadSize);
    }

    void testWebsocketParserAndSerializer(
        int readSize, int frameCount, FrameType type, bool masked, unsigned int mask)
    {
        int messageOffset = 0;
        auto message = prepareMessage(
            m_defaultPayload, frameCount, type, masked, mask, GetParam().compressionType);

        for (int i = 0; ; ++i)
        {
            int consumeLen = std::min((int)(message.size() - messageOffset), readSize);
            m_parser.consume(message.data() + i * readSize, consumeLen);
            messageOffset += consumeLen;
            if (messageOffset >= message.size())
                break;
        }

        ASSERT_TRUE(std::any_of(
            m_frameTypes.cbegin(), m_frameTypes.cend(),
            [type](auto t) {return t == type; }));
        ASSERT_EQ(m_defaultPayload, m_payload);

        if (frameCount > 1)
        {
            ASSERT_TRUE(m_fin);
        }

        m_frameTypes.clear();
    }

private:
    Parser m_parser;
    std::vector<FrameType> m_frameTypes;
    nx::Buffer m_payload;
    bool m_fin;
    nx::Buffer m_defaultPayload;
};

TEST_P(WebsocketParserTest, NoMask)
{
    testWebsocketParserAndSerializer(26, 1, FrameType::binary, false, 0);
    testWebsocketParserAndSerializer(13, 2, FrameType::binary, false, 0);
    testWebsocketParserAndSerializer(25, 91, FrameType::binary, false, 0);
    testWebsocketParserAndSerializer(256, 32, FrameType::binary, false, 0);
}

TEST_P(WebsocketParserTest, Mask)
{
    testWebsocketParserAndSerializer(13, 1, FrameType::binary, true, 0xfa121a23);
    testWebsocketParserAndSerializer(10, 5, FrameType::text, true, 0xd903);
    testWebsocketParserAndSerializer(1, 7, FrameType::binary, true, 0xfa121423);
    testWebsocketParserAndSerializer(13, 33, FrameType::binary, true, 0xfa121a23);
    testWebsocketParserAndSerializer(25, 91, FrameType::binary, true, 0xfa121423);
    testWebsocketParserAndSerializer(256, 32, FrameType::binary, true, 0xd903);
}

INSTANTIATE_TEST_CASE_P(WebsocketParserSerializer_differentCompressionModes,
    WebsocketParserTest,
    ::testing::Values(
        ParserTestParams(CompressionType::none, 20),
        ParserTestParams(CompressionType::perMessageDeflate, 20),
        ParserTestParams(CompressionType::none, 1000),
        ParserTestParams(CompressionType::perMessageDeflate, 1000)));
