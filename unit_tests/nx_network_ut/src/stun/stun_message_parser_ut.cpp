#include <gtest/gtest.h>

#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>

namespace nx {
namespace stun {
namespace test {

class StunMessageParser:
    public ::testing::Test
{
protected:
    void whenParseMultipleJunkPackets()
    {
        for (int i = 0; i < 10; ++i)
            whenParseJunkPacket();
    }

    void whenParseJunkPacket()
    {
        m_bufferToParse = nx::utils::random::generate(
            nx::utils::random::number<int>(1, 1024));

        whenPacketIsInvoked();
        thenParseDidNotSucceed();
    }

    void whenPacketIsInvoked()
    {
        m_parsedMessage = Message();
        m_parser.setMessage(&m_parsedMessage);
        std::size_t bytesRead = 0;
        m_prevParseResult = m_parser.parse(m_bufferToParse, &bytesRead);
    }

    void thenParserIsAbleToParseCorrectPacket()
    {
        prepareCorrectStunPacket();
        whenPacketIsInvoked();
        thenParseSucceeded();
    }

    void thenParseSucceeded()
    {
        ASSERT_EQ(nx::network::server::ParserState::done, m_prevParseResult);
    }

    void thenParseDidNotSucceed()
    {
        ASSERT_NE(nx::network::server::ParserState::done, m_prevParseResult);
    }

private:
    Message m_messageToParse;
    Message m_parsedMessage;
    MessageParser m_parser;
    nx::Buffer m_bufferToParse;
    nx::network::server::ParserState m_prevParseResult = nx::network::server::ParserState::init;

    void prepareCorrectStunPacket()
    {
        m_messageToParse = Message(Header(MessageClass::request, MethodType::bindingMethod));
        m_messageToParse.header.transactionId = 
            nx::utils::generateRandomName(Header::TRANSACTION_ID_SIZE);

        MessageSerializer serializer;
        serializer.setMessage(&m_messageToParse);
        m_bufferToParse.clear();
        m_bufferToParse.reserve(16 * 1024);
        size_t serializedSize = 0;
        ASSERT_EQ(
            serializer.serialize(&m_bufferToParse, &serializedSize),
            nx::network::server::SerializerState::done);
    }
};

TEST_F(StunMessageParser, is_able_to_parse_after_junk_message)
{
    whenParseMultipleJunkPackets();
    thenParserIsAbleToParseCorrectPacket();
}

} // namespace test
} // namespace stun
} // namespace nx
