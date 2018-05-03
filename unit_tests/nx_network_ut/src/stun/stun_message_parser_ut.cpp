#include <gtest/gtest.h>

#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/stun/stun_attributes.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

class StunMessageParser:
    public ::testing::Test
{
public:
    StunMessageParser():
        m_userName("aaa"),
        m_userPassword("bbb")
    {
    }

protected:
    void prepareMessageWithFingerprint()
    {
        prepareCorrectTestMessage();
        m_testMessage.newAttribute<attrs::FingerPrint>();
    }

    void addIntegrity()
    {
        m_testMessage.insertIntegrity(m_userName, m_userPassword);
    }

    void whenParseMultipleJunkPackets()
    {
        for (int i = 0; i < 10; ++i)
            whenParseJunkPacket();
    }

    void whenParseJunkPacket()
    {
        m_bufferToParse = nx::utils::random::generate(
            nx::utils::random::number<int>(1, 1024));

        whenParsePacket();
        thenParseDidNotSucceed();
    }

    void whenParsePacket()
    {
        m_parsedMessage = Message();
        m_parser.setMessage(&m_parsedMessage);
        std::size_t bytesRead = 0;
        m_prevParseResult = m_parser.parse(m_bufferToParse, &bytesRead);
    }

    void thenParserIsAbleToParseCorrectPacket()
    {
        prepareCorrectTestMessage();
        serializeMessage();

        whenParsePacket();

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

    void serializeMessage()
    {
        MessageSerializer serializer;
        serializer.setMessage(&m_testMessage);
        m_bufferToParse.clear();
        m_bufferToParse.reserve(16 * 1024);
        size_t serializedSize = 0;
        ASSERT_EQ(
            nx::network::server::SerializerState::done,
            serializer.serialize(&m_bufferToParse, &serializedSize));
    }

    void assertMessageCanBeParsed()
    {
        whenParsePacket();
        thenParseSucceeded();
    }

    void assertIntegrityCheckPasses()
    {
        m_parsedMessage.verifyIntegrity(m_userName, m_userPassword);
    }

private:
    Message m_testMessage;
    Message m_parsedMessage;
    MessageParser m_parser;
    nx::Buffer m_bufferToParse;
    nx::network::server::ParserState m_prevParseResult = nx::network::server::ParserState::init;
    nx::String m_userName;
    nx::String m_userPassword;

    void prepareCorrectTestMessage()
    {
        m_testMessage = Message(Header(MessageClass::request, MethodType::bindingMethod));
        m_testMessage.header.transactionId =
            nx::utils::generateRandomName(Header::TRANSACTION_ID_SIZE);
    }
};

TEST_F(StunMessageParser, is_able_to_parse_after_junk_message)
{
    whenParseMultipleJunkPackets();
    thenParserIsAbleToParseCorrectPacket();
}

TEST_F(StunMessageParser, fingerprint)
{
    prepareMessageWithFingerprint();
    serializeMessage();
    assertMessageCanBeParsed();
}

TEST_F(StunMessageParser, fingerprint_and_integrity)
{
    prepareMessageWithFingerprint();
    addIntegrity();

    serializeMessage();

    assertMessageCanBeParsed();
    assertIntegrityCheckPasses();
}

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
