#include <vector>
#include <algorithm>
#include <iterator>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/network/websocket/websocket_parser.h>

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

int calcHeaderSize(bool masked, int payloadLenType)
{
    int result = 2;
    result += masked ? 4 : 0;
    result += payloadLenType <= 125 ? 0 : payloadLenType == 126 ? 2 : 8;

    return result;
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

int payloadLenTypeByLen(int64_t len)
{
    if (len <= 125)
        return len;
    else if (len < 65536)
        return 126;
    else
        return 127;
}

//nx::Buffer createFrame()
//{
//}

std::vector<char> prepareMessage(const std::vector<char>& payload, int frameCount, FrameType type, bool masked, int mask)
{
    std::vector<char> result;
    std::vector<char> headerBuf;
    int payloadSize = payload.size();
    int frameLen = payloadSize / frameCount;
    int payloadLenType = payloadLenTypeByLen(frameLen);
    auto headerSize = calcHeaderSize(masked, payloadLenType);

    result.resize(payload.size() + headerSize*frameCount);
    for (int i = 0; i < frameCount; ++ i)
    {
        headerBuf.assign(headerSize, 0);
        fillHeader(headerBuf.data(), (i == frameCount - 1) ? true : false, 
            (i > 0 ? FrameType::continuation : type), payloadLenType, frameLen, masked, mask);
        std::copy(headerBuf.cbegin(), headerBuf.cend(), result.begin() + i*frameLen + i*headerSize);
        std::copy(payload.cbegin() + i*frameLen, payload.cbegin() + (i + 1)*frameLen, result.begin() + i*frameLen + (i + 1)*headerSize);
    }

    return result;
}

TEST(WebsocketParser, BinaryMessage_2Frames_LengthShort_NoMask)
{
    TestParserHandler ph;
    Parser p(Role::client, &ph);
    const int kMessageSize = 1008;

    EXPECT_CALL(ph, frameStarted(FrameType::binary, false)).Times(1);
    EXPECT_CALL(ph, frameStarted(FrameType::continuation, true)).Times(1);
    EXPECT_CALL(ph, framePayload(_, _)).Times(AtLeast(1));
    EXPECT_CALL(ph, frameEnded()).Times(2);
    EXPECT_CALL(ph, messageEnded()).Times(1);

    std::vector<char> payload;
    payload.resize(1000);
    for (int i = 0; i < 1000; i += 5)
        memcpy(payload.data() + i, "hello", 5);

    int messageOffset = 0;
    auto message = prepareMessage(payload, 2, FrameType::binary, false, 0);

    for (int i = 0; ; ++i) 
    {
        int consumeLen = std::min(kMessageSize - messageOffset, 13);
        p.consume(message.data() + i * 13, consumeLen);
        messageOffset += consumeLen;
        if (messageOffset >= kMessageSize)
            break;
    }
}