#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/network/websocket/parser.h>

using namespace nx::network::websocket;

class TestParserHandler : public ParserHandler 
{
public:
    MOCK_METHOD2(frameStarted, void(FrameType type, bool fin));
    MOCK_METHOD2(framePayload, void(const char* data, int64_t len));
    MOCK_METHOD0(frameEnded, void());
    MOCK_METHOD0(messageEnded, void());
    MOCK_METHOD1(handleError, void(Error err));
};

unsigned char kShortTextMessageFinNoMask[] = { 0x81, 0x5, 'h', 'e', 'l', 'l', 'o' };
using ::testing::_;
using ::testing::AtLeast;

TEST(WebsocketParser, SimpleTextMessage_OneBuffer)
{
    TestParserHandler ph;
    EXPECT_CALL(ph, frameStarted(FrameType::text, true)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 2, 5)).Times(1);
    EXPECT_CALL(ph, frameEnded()).Times(1);
    EXPECT_CALL(ph, messageEnded()).Times(1);
    EXPECT_CALL(ph, handleError(_)).Times(0);

    Parser p(Role::client, &ph);
    p.consume((char*)kShortTextMessageFinNoMask, sizeof(kShortTextMessageFinNoMask));
}

TEST(WebsocketParser, SimpleTestMessage_SeveralParts)
{
    TestParserHandler ph;
    EXPECT_CALL(ph, frameStarted(FrameType::text, true)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 2, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 3, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 4, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 5, 1)).Times(1);
    EXPECT_CALL(ph, framePayload((const char*)kShortTextMessageFinNoMask + 6, 1)).Times(1);

    EXPECT_CALL(ph, frameEnded()).Times(1);
    EXPECT_CALL(ph, messageEnded()).Times(1);
    EXPECT_CALL(ph, handleError(_)).Times(0);

    Parser p(Role::client, &ph);
    for (int i = 0; i < sizeof(kShortTextMessageFinNoMask); ++i)
        p.consume((char*)kShortTextMessageFinNoMask + i, 1);
}

int fillHeader(char* data, bool fin, int opCode, int payloadLenType, int64_t payloadLen, bool masked, int mask)
{
    data[0] |= (int)fin << 7;
    data[0] |= opCode & 0xf;
    data[1] |= (int)masked << 7;
    data[1] |= payloadLenType & 0x7f;

    int maskOffset = 2;
    if (payloadLenType == 126)
    {
        *reinterpret_cast<unsigned short*>(data + 2) = htons(payloadLen);
        maskOffset = 4;
    }
    else
    {
        *reinterpret_cast<uint64_t*>(data + 2) = htonll(payloadLen);
        maskOffset = 10;
    }

    if (masked)
        *reinterpret_cast<uint64_t*>(data + maskOffset) = mask;

    return maskOffset;
}

TEST(WebsocketParser, BinaryMessage_2Frames_LengthShort_NoMask)
{
    TestParserHandler ph;
    Parser p(Role::client, &ph);
    const int kMessageSize = 1008;
    std::vector<char> message(kMessageSize, 0);

    ASSERT_EQ(fillHeader(message.data(), false, FrameType::binary, 126, 500, false, 0), 4);
    for (int i = 0; i < 500; i += 5)
        memcpy(message.data() + i + 4, "hello", 5);

    ASSERT_EQ(fillHeader(message.data() + 504, true, FrameType::binary, 126, 500, false, 0), 4);
    for (int i = 0; i < 500; i += 5)
        memcpy(message.data() + i + 508, "hello", 5);

    EXPECT_CALL(ph, frameStarted(FrameType::binary, false)).Times(1);
    EXPECT_CALL(ph, frameStarted(FrameType::binary, true)).Times(1);
    EXPECT_CALL(ph, framePayload(_, _)).Times(AtLeast(1));
    EXPECT_CALL(ph, frameEnded()).Times(2);
    EXPECT_CALL(ph, messageEnded()).Times(1);

    int messageOffset = 0;
    for (int i = 0; ; ++i) 
    {
        int consumeLen = std::min(kMessageSize - messageOffset, 13);
        p.consume(message.data() + i * 13, consumeLen);
        messageOffset += consumeLen;
        if (messageOffset >= kMessageSize)
            break;
    }
}