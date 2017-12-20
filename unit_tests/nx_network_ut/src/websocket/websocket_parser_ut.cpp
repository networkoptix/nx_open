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
    MOCK_METHOD2(frameStarted, void(FrameType type, bool fin));
    MOCK_METHOD2(framePayload, void(const char* data, int len));
    MOCK_METHOD0(frameEnded, void());
    MOCK_METHOD0(messageEnded, void());
    MOCK_METHOD1(handleError, void(Error err));
};

namespace {

unsigned char kShortTextMessageFinNoMask[] = { 0x81, 0x5, 'h', 'e', 'l', 'l', 'o' };
using ::testing::_;
using ::testing::AtLeast;

static std::vector<char> kDefaultPayload;
static std::once_flag payloadInitOnceFlag;

static std::vector<char> prepareMessage(const std::vector<char>& payload, int frameCount, FrameType type, bool masked, int mask)
{
    std::vector<char> result;
    Serializer serializer(masked, mask);

    int payloadSize = (int)payload.size();
    int frameLen = payloadSize / frameCount;
    int resultSize = frameCount * serializer.prepareFrame(nullptr, frameLen, type, true, nullptr, 0);
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
        currentPos += serializer.prepareFrame(payload.data() + currentPayloadPos,
            currentFrameLen, (FrameType)opCode, i == frameCount - 1,
            result.data() + currentPos, (int)result.size() - currentPos);
        currentPayloadPos += currentFrameLen;
    }

    return result;
}

static void fillDummyPayload(std::vector<char>* payload, int size)
{
    static const char* const kPattern = "hello";
    static const int kPatternSize = (int)std::strlen(kPattern);

    payload->resize((size_t)size);
    char* pdata = payload->data();

    while (size > 0)
    {
        int copySize = std::min(kPatternSize, size);
        memcpy(pdata, kPattern, copySize);
        size -= copySize;
        pdata += copySize;
    }
}

}

class WebsocketParserTest : public ::testing::Test
{
protected:
    WebsocketParserTest(): p(Role::client, &ph)
    {
        std::call_once(payloadInitOnceFlag, []() { fillDummyPayload(&kDefaultPayload, 1000); });
    }

    void testWebsocketParserAndSerializer(
        int readSize,
        int frameCount,
        FrameType type,
        bool masked,
        unsigned int mask,
        const std::vector<char>& payload = kDefaultPayload)
    {
        EXPECT_CALL(ph, frameStarted(type, frameCount == 1 ? true : false)).Times(1);
        EXPECT_CALL(ph, frameStarted(FrameType::continuation, _)).Times(AtLeast(frameCount == 1 ? 0 : 1));
        EXPECT_CALL(ph, framePayload(_, _)).Times(AtLeast(1));
        EXPECT_CALL(ph, frameEnded()).Times(frameCount);
        EXPECT_CALL(ph, messageEnded()).Times(1);

        size_t messageOffset = 0;
        auto message = prepareMessage(payload, frameCount, type, masked, mask);

        for (int i = 0; ; ++i)
        {
            int consumeLen = std::min((int)(message.size() - messageOffset), readSize);
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

    for (size_t i = 0; i < sizeof(kShortTextMessageFinNoMask); ++i)
        p.consume((char*)kShortTextMessageFinNoMask + i, 1);
}

TEST_F(WebsocketParserTest, BinaryMessage_2Frames_LengthShort_NoMask)
{
    testWebsocketParserAndSerializer(13, 2, FrameType::binary, false, 0);
}

TEST_F(WebsocketParserTest, MultipleFrames_LengthShort_Mask)
{
    testWebsocketParserAndSerializer(10, 5, FrameType::text, true, 0xd903);
    testWebsocketParserAndSerializer(1, 7, FrameType::binary, true, 0xfa121423);
    testWebsocketParserAndSerializer(13, 33, FrameType::binary, true, 0xfa121a23);
}

TEST_F(WebsocketParserTest, MultipleFrames_Length64)
{
    std::vector<char> largePayload;
    fillDummyPayload(&largePayload, 100 * 1024);

    testWebsocketParserAndSerializer(10, 5, FrameType::text, true, 0xd903, largePayload);
    testWebsocketParserAndSerializer(1, 7, FrameType::binary, true, 0xfa121423, largePayload);
    testWebsocketParserAndSerializer(13, 33, FrameType::binary, true, 0xfa121a23, largePayload);
}
