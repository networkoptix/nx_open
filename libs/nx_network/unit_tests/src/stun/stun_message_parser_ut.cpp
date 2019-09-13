#include <random>

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

static constexpr int kIntAttributeType = attrs::userDefined + 1;
static constexpr int kStringAttributeType = attrs::userDefined + 2;

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
        prepareRandomMessage();
        m_testMessage.newAttribute<attrs::FingerPrint>();
    }

    void prepareRandomMessage()
    {
        std::mt19937 randomDevice(::testing::UnitTest::GetInstance()->random_seed());
        std::uniform_int_distribution<> distribution(1, 1000);

        m_testMessage = Message(Header(MessageClass::request, MethodType::bindingMethod));
        m_testMessage.header.transactionId =
            nx::utils::generateRandomName(Header::TRANSACTION_ID_SIZE);

        for (int i = 0; i < 11; ++i)
        {
            if (distribution(randomDevice) % 2)
            {
                m_testMessage.addAttribute(kIntAttributeType, distribution(randomDevice));
            }
            else
            {
                m_testMessage.newAttribute<attrs::Unknown>(
                    kStringAttributeType,
                    nx::utils::generateRandomName(7));
            }
        }
    }

    void addIntegrity()
    {
        m_testMessage.insertIntegrity(m_userName, m_userPassword);
    }

    void whenParseMultipleJunkPackets()
    {
        for (int i = 0; i < 10; ++i)
        {
            whenParseJunkPacket();
            m_parser.reset();
        }
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

    void whenParsePacketByRandomFragments()
    {
        m_parsedMessage = Message();
        m_parser.setMessage(&m_parsedMessage);

        nx::Buffer buf = m_bufferToParse;
        while (!buf.isEmpty() && m_prevParseResult != nx::network::server::ParserState::done)
        {
            const auto fragmentSize = nx::utils::random::number<int>(1, buf.size());

            std::size_t bytesRead = 0;
            nx::Buffer fragment = buf.mid(0, fragmentSize);
            m_prevParseResult = m_parser.parse(fragment, &bytesRead);
            ASSERT_EQ(fragment.size(), bytesRead);
            buf.remove(0, bytesRead);
        }
    }

    void thenParserIsAbleToParseCorrectPacket()
    {
        prepareRandomMessage();
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

    void andParsedMessageIsExpected()
    {
        assertEqual(m_testMessage, m_parsedMessage);
    }

private:
    Message m_testMessage;
    Message m_parsedMessage;
    MessageParser m_parser;
    nx::Buffer m_bufferToParse;
    nx::network::server::ParserState m_prevParseResult = nx::network::server::ParserState::init;
    nx::String m_userName;
    nx::String m_userPassword;

    void assertEqual(const Message& one, const Message& two)
    {
        assertEqual(one.header, two.header);
        assertEqual(one.attributes, two.attributes);
    }

    void assertEqual(const Header& one, const Header& two)
    {
        ASSERT_EQ(one.messageClass, two.messageClass);
        ASSERT_EQ(one.method, two.method);
        ASSERT_EQ(one.transactionId, two.transactionId);
    }

    void assertEqual(const Message::AttributesMap& one, const Message::AttributesMap& two)
    {
        ASSERT_EQ(one.size(), two.size());

        for (auto it1 = one.begin(), it2 = two.begin();
            it1 != one.end() && it2 != two.end();
            ++it1, ++it2)
        {
            ASSERT_EQ(it1->first, it2->first);
            ASSERT_EQ(it1->second->getType(), it2->second->getType());
            if (it1->second->getType() == kIntAttributeType)
            {
                ASSERT_EQ(
                    static_cast<attrs::IntAttribute*>(it1->second.get())->value(),
                    static_cast<attrs::IntAttribute*>(it2->second.get())->value());
            }
            else if (it1->second->getType() == kStringAttributeType)
            {
                ASSERT_EQ(
                    static_cast<attrs::Unknown*>(it1->second.get())->getBuffer(),
                    static_cast<attrs::Unknown*>(it2->second.get())->getBuffer());
            }
            else
            {
                FAIL();
            }
        }
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

TEST_F(StunMessageParser, parses_fragmented_message)
{
    prepareRandomMessage();
    serializeMessage();

    whenParsePacketByRandomFragments();

    thenParseSucceeded();
    andParsedMessageIsExpected();
}

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
