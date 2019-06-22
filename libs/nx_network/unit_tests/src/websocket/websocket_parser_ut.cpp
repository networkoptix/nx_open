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

class TestParserHandler : public ParserHandler
{
public:
    MOCK_METHOD3(gotFrame, void(FrameType type, const nx::Buffer& data, bool fin));
    MOCK_METHOD1(handleError, void(Error err));
};

namespace {

using ::testing::_;
using ::testing::AtLeast;

static nx::Buffer prepareMessage(
    const nx::Buffer& payload, int frameCount, FrameType type, bool masked, int mask,
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

        result += serializer.prepareFrame(
            payload, opCode, compressionType, i == frameCount - 1, i == 0);
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

class WebsocketParserTest : public ::testing::TestWithParam<CompressionType>
{
protected:
    WebsocketParserTest(): m_parser(Role::client, &m_parserHandler, GetParam())
    {
        m_defaultPayload = fillDummyPayload(1000);
    }

    void testWebsocketParserAndSerializer(
        int readSize, int frameCount, FrameType type, bool masked, unsigned int mask)
    {
        EXPECT_CALL(
            m_parserHandler, gotFrame(type, m_defaultPayload, _)).Times(1);

        if (frameCount > 1)
        {
            EXPECT_CALL(
                m_parserHandler, gotFrame(FrameType::continuation, m_defaultPayload, true))
                .Times(1);
        }

        if (frameCount > 2)
        {
            EXPECT_CALL(
                m_parserHandler, gotFrame(FrameType::continuation, m_defaultPayload, false))
                .Times(frameCount - 2);
        }

        int messageOffset = 0;
        auto message = prepareMessage(m_defaultPayload, frameCount, type, masked, mask, GetParam());

        for (int i = 0; ; ++i)
        {
            int consumeLen = std::min((int)(message.size() - messageOffset), readSize);
            m_parser.consume(message.data() + i * readSize, consumeLen);
            messageOffset += consumeLen;
            if (messageOffset >= message.size())
                break;
        }
    }

private:
    TestParserHandler m_parserHandler;
    Parser m_parser;
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
    ::testing::Values(CompressionType::none, CompressionType::perMessageDeflate));
