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