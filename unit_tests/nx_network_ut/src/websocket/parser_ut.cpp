#include <vector>
#include <algorithm>
#include <iterator>
#include <thread>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/network/websocket/websocket_parser.h>
#include <nx/network/websocket/websocket_serializer.h>

using namespace nx::network::websocket;

class TestParserHandler : public ParserHandler 
{
public:
    MOCK_METHOD2(frameStarted, void(FrameType type, bool fin));
    MOCK_METHOD2(framePayload, void(const char* data, int len));
    MOCK_METHOD0(frameEnded, void());
    MOCK_METHOD0(messageEnded, void());
    MOCK_METHOD1(handleError, void(Error err));
};

unsigned char kShortTextMessageFinNoMask[] = { 0x81, 0x5, 'h', 'e', 'l', 'l', 'o' };
using ::testing::_;
using ::testing::AtLeast;

std::vector<char> payload;
std::once_flag payloadInitOnceFlag;

std::vector<char> prepareMessage(const std::vector<char>& payload, int frameCount, FrameType type, bool masked, int mask)
{
    std::vector<char> result;
    int payloadSize = payload.size();
    int frameLen = payloadSize / frameCount;
    int resultSize = frameCount * prepareFrame(nullptr, frameLen, type, true, masked, mask, nullptr, 0);
    int frameTailSize = payloadSize % frameCount;
    resultSize += frameTailSize;
    result.assign(resultSize, 0);

    int currentPos = 0;
    int currentPayloadPos = 0;
    for (int i = 0; i < frameCount; ++i)
    {
        FrameType opCode = type;
        if (i > 0)
            opCode = FrameType::continuation;
        int currentFrameLen = i == 0 ? frameLen + frameTailSize : frameLen;
        currentPos += prepareFrame(payload.data() + currentPayloadPos,
            currentFrameLen, (FrameType)opCode, i == frameCount - 1,
            masked, mask, result.data() + currentPos, result.size() - currentPos);
        currentPayloadPos += currentFrameLen;
    }

    return result;
}

class WebsocketParserTest : public ::testing::Test
{
protected:
    WebsocketParserTest(): p(Role::client, &ph) 
    {
        std::call_once(payloadInitOnceFlag, []()
            {
                payload.resize(1000);
                for (int i = 0; i < 1000; i += 5)
                    memcpy(payload.data() + i, "hello", 5);
            });
    }

    void testWebsocketParserAndSerializer(int readSize, int frameCount, FrameType type, bool masked, unsigned int mask)
    {
        EXPECT_CALL(ph, frameStarted(type, frameCount == 1 ? true : false)).Times(1);
        EXPECT_CALL(ph, frameStarted(FrameType::continuation, true)).Times(AtLeast(frameCount == 1 ? 0 : 1));
        EXPECT_CALL(ph, framePayload(_, _)).Times(AtLeast(1));
        EXPECT_CALL(ph, frameEnded()).Times(frameCount);
        EXPECT_CALL(ph, messageEnded()).Times(1);

        int messageOffset = 0;
        auto message = prepareMessage(payload, frameCount, type, masked, mask);

        for (int i = 0; ; ++i)
        {
            int consumeLen = std::min((int)message.size() - messageOffset, readSize);
            p.consume(message.data() + i * readSize, consumeLen);
            messageOffset += consumeLen;
            if (messageOffset >= message.size())
                break;
        }
    }

    TestParserHandler ph;
    Parser p;
};

TEST_F(WebsocketParserTest, SimpleTextMessage_OneBuffer)
{
    EXPECT_CALL(ph, frameStarted(FrameType::text, true)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 2, 5)).Times(1);
    EXPECT_CALL(ph, frameEnded()).Times(1);
    EXPECT_CALL(ph, messageEnded()).Times(1);
    EXPECT_CALL(ph, handleError(_)).Times(0);

    p.consume((char*)kShortTextMessageFinNoMask, sizeof(kShortTextMessageFinNoMask));
}

TEST_F(WebsocketParserTest, SimpleTestMessage_SeveralParts)
{
    EXPECT_CALL(ph, frameStarted(FrameType::text, true)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 2, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 3, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 4, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 5, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 6, 1)).Times(1);

    EXPECT_CALL(ph, frameEnded()).Times(1);
    EXPECT_CALL(ph, messageEnded()).Times(1);
    EXPECT_CALL(ph, handleError(_)).Times(0);

    for (int i = 0; i < sizeof(kShortTextMessageFinNoMask); ++i)
        p.consume((char*)kShortTextMessageFinNoMask + i, 1);
}

TEST_F(WebsocketParserTest, BinaryMessage_2Frames_LengthShort_NoMask)
{
    testWebsocketParserAndSerializer(13, 2, FrameType::binary, false, 0);
}

TEST_F(WebsocketParserTest, TextMessage_5Frames_LengthShort_Mask)
{
    testWebsocketParserAndSerializer(1, 5, FrameType::text, false, 0xd903);
}