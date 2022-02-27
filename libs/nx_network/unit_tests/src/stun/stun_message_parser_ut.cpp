// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
static constexpr int kStringAttributeType = attrs::userDefined + 100;

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
        addFingerprint();
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
                m_testMessage.addAttribute(kIntAttributeType + i, distribution(randomDevice));
            }
            else
            {
                m_testMessage.newAttribute<attrs::Unknown>(
                    kStringAttributeType + i,
                    nx::Buffer(nx::utils::generateRandomName(7)));
            }
        }
    }

    void addFingerprint()
    {
        m_testMessage.newAttribute<attrs::FingerPrint>();
    }

    void addZeroLengthAttribute()
    {
        m_testMessage.newAttribute<attrs::Unknown>(attrs::userDefined + 151, nx::Buffer());
    }

    void prepareRandomMessageWithoutAttributes()
    {
        prepareRandomMessage();
        m_testMessage.attributes.clear();
    }

    void prepareSerializedMessageWithDuplicateAttributes()
    {
        prepareRandomMessage();
        serializeMessage();

        // Copying attributes.
        m_bufferToParse.append(m_bufferToParse.substr(MessageParser::kHeaderSize));

        // Adjusting message length.
        std::uint16_t newMessageLength = htons(m_bufferToParse.size() - MessageParser::kHeaderSize);
        memcpy(m_bufferToParse.data() + 0x02, &newMessageLength, sizeof(newMessageLength));
    }

    void addIntegrity()
    {
        m_testMessage.insertIntegrity(m_userName, m_userPassword);
    }

    void givenCorruptedMessageWithValidFingerprint()
    {
        prepareMessageWithFingerprint();
        serializeMessage();
        whenCorruptSerializedMessage();
    }

    void givenCorruptedMessageWithSmallerLength()
    {
        m_bufferToParse = nx::utils::fromHex(nx::Buffer(
            "0111" "0008" "2112A442"         // response: binding, length=20, magic cookie
            "012345670123456789ABCDEF"      // TID
            "9001" "0006" "202020202020" "0000"));    // string attr of size 6 (6 spaces) and 2-byte padding.
    }

    void givenSerializedMessage(nx::Buffer msg)
    {
        m_bufferToParse = std::move(msg);
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
            nx::utils::random::number<int>(1, 1024)).toStdString();

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
        while (!buf.empty() && m_prevParseResult != nx::network::server::ParserState::done)
        {
            const auto fragmentSize = nx::utils::random::number<int>(1, buf.size());

            std::size_t bytesRead = 0;
            nx::Buffer fragment = buf.substr(0, fragmentSize);
            m_prevParseResult = m_parser.parse(fragment, &bytesRead);
            ASSERT_EQ(fragment.size(), bytesRead);
            buf.erase(0, bytesRead);
        }
    }

    void whenParseMalformedHeader()
    {
        m_parsedMessage = Message();
        m_parser.setMessage(&m_parsedMessage);

        std::size_t bytesRead = 0;
        m_prevParseResult = m_parser.parse("malformed header", &bytesRead);
    }

    void whenParseBuffer(const nx::Buffer& buf)
    {
        m_parsedMessage = Message();
        m_parser.setMessage(&m_parsedMessage);

        std::size_t bytesRead = 0;
        m_prevParseResult = m_parser.parse(buf, &bytesRead);
    }

    void whenCorruptSerializedMessage()
    {
        // Not corrupting FINGERPRINT attribute.
        ASSERT_GT(m_bufferToParse.size(), 8);
        m_bufferToParse[rand() % (m_bufferToParse.size() - 8)] = rand();
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

    void thenParseFailed()
    {
        ASSERT_EQ(nx::network::server::ParserState::failed, m_prevParseResult);
    }

    void thenParseDidNotSucceed()
    {
        ASSERT_NE(nx::network::server::ParserState::done, m_prevParseResult);
    }

    void serializeMessage()
    {
        MessageSerializer serializer;
        serializer.setAlwaysAddFingerprint(m_alwaysAddFingerprint);
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
        // The fingerprint is validated by the parser.
        m_testMessage.attributes.erase(attrs::fingerPrint);
        m_parsedMessage.attributes.erase(attrs::fingerPrint);

        assertEqual(m_testMessage, m_parsedMessage);
    }

    MessageParser& parser()
    {
        return m_parser;
    }

    void setAlwaysAddFingerprint(bool val)
    {
        m_alwaysAddFingerprint = val;
    }

private:
    Message m_testMessage;
    Message m_parsedMessage;
    MessageParser m_parser;
    nx::Buffer m_bufferToParse;
    nx::network::server::ParserState m_prevParseResult = nx::network::server::ParserState::init;
    std::string m_userName;
    std::string m_userPassword;
    bool m_alwaysAddFingerprint = false;

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
            if (it1->second->getType() >= kStringAttributeType)
            {
                ASSERT_EQ(
                    static_cast<attrs::Unknown*>(it1->second.get())->getBuffer(),
                    static_cast<attrs::Unknown*>(it2->second.get())->getBuffer());
            }
            else if (it1->second->getType() >= kIntAttributeType)
            {
                ASSERT_EQ(
                    static_cast<attrs::IntAttribute*>(it1->second.get())->value(),
                    static_cast<attrs::IntAttribute*>(it2->second.get())->value());
            }
            else
            {
                FAIL();
            }
        }
    }
};

TEST_F(StunMessageParser, properly_parses_message_with_no_attributes)
{
    prepareRandomMessageWithoutAttributes();

    serializeMessage();

    assertMessageCanBeParsed();
    andParsedMessageIsExpected();
}

TEST_F(StunMessageParser, properly_parses_message_with_attributes)
{
    prepareRandomMessage();

    serializeMessage();

    assertMessageCanBeParsed();
    andParsedMessageIsExpected();
}

TEST_F(StunMessageParser, is_able_to_parse_after_junk_message)
{
    whenParseMultipleJunkPackets();
    thenParserIsAbleToParseCorrectPacket();
}

TEST_F(StunMessageParser, message_with_valid_fingerprint_is_parsed_successfully)
{
    prepareMessageWithFingerprint();
    serializeMessage();
    assertMessageCanBeParsed();
}

TEST_F(StunMessageParser, message_with_zero_length_attribute_and_valid_fingerprint)
{
    prepareRandomMessageWithoutAttributes();
    addZeroLengthAttribute();
    addFingerprint();

    serializeMessage();

    assertMessageCanBeParsed();
}

TEST_F(StunMessageParser, message_without_attributes_and_with_fingerprint)
{
    prepareRandomMessageWithoutAttributes();

    setAlwaysAddFingerprint(true);
    serializeMessage();

    assertMessageCanBeParsed();
    andParsedMessageIsExpected();
}

TEST_F(StunMessageParser, message_without_fingerprint_causes_parse_error_when_in_corresponding_mode)
{
    parser().setFingerprintRequired(true);
    prepareRandomMessage();
    serializeMessage();

    whenParsePacket();

    thenParseFailed();
}

TEST_F(StunMessageParser, fingerprint_and_integrity)
{
    prepareMessageWithFingerprint();
    addIntegrity();

    serializeMessage();

    assertMessageCanBeParsed();
    assertIntegrityCheckPasses();
}

TEST_F(StunMessageParser, message_with_wrong_fingerprint_causes_parse_error)
{
    givenCorruptedMessageWithValidFingerprint();
    whenParsePacket();
    thenParseFailed();
}

TEST_F(StunMessageParser, message_with_smaller_length_causes_parse_error)
{
    givenCorruptedMessageWithSmallerLength();
    whenParsePacket();
    thenParseFailed();
}

TEST_F(StunMessageParser, message_with_wrong_length_and_fingerprint_causes_parse_error)
{
    // The following message has length field corrupted (it exceeds the actual message length).
    // So, the fingerprint is invalid. Expecting the parser to reach the fingerprint and fail.
    givenSerializedMessage(nx::utils::fromBase64(nx::Buffer(
        "AAEsHCESpEJhZU9pWUxBbEpaWErgAQAEAAABAuACAAdNNG5nWmVJAIAoAASxXhGu")));

    whenParsePacket();
    thenParseFailed();
}

TEST_F(StunMessageParser, parses_fragmented_message)
{
    prepareRandomMessage();
    serializeMessage();

    whenParsePacketByRandomFragments();

    thenParseSucceeded();
    andParsedMessageIsExpected();
}

TEST_F(StunMessageParser, detects_malformed_header)
{
    whenParseMalformedHeader();
    thenParseFailed();
}

/**
 * This message has inconsistent attribute size: it conflicts with the message header.
 */
static constexpr char kCorruptedMessage[] =
    "AKYA9CESpEIdpZkCcDPOqQY2DZPgAwAyZGNfMWY1M2Q1OTUtMTQxNC00YWVkLTlhOWMtZTQzODAzMDJhNWFhX"
    "zEwOTY5ODMzOTYAAOAEACZ7MDk1YmIzYjAtZGU3MC00YjFiLTgzOGUtZjg1MWU2MDIzODhjfQAA4AUABAAAAA"
    "TiAABJMWZjYzhkN2UtNDQ0Zi00MmRhLTdjNTEtM2M3MjI2MTZmMjE0LmJjYzAyOWI3LTljNDctNDU0Yy1iZWR"
    "iLWRlNzUzZjg4MmRmZAAAAOIEACE2NS4xMjQuMjQ2LjE3NSwxNzIuMjMuNC4yMzc6NTM3N1ZOZW0mmVBlALtO"
    "BevgR1K2RtD2";

TEST_F(StunMessageParser, corrupted_message_results_in_parser_error)
{
    whenParseBuffer(nx::utils::fromBase64(nx::Buffer(kCorruptedMessage)));
    thenParseFailed();
}

TEST_F(StunMessageParser, parses_message_with_duplicate_attributes)
{
    prepareSerializedMessageWithDuplicateAttributes();
    assertMessageCanBeParsed();
}

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
