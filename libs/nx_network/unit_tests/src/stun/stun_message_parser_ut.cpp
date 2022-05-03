// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <random>

#include <gtest/gtest.h>

#include <nx/network/stun/message_integrity.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/stun/stun_attributes.h>
#include <nx/utils/auth/utils.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>

namespace nx::network::stun::test {

static constexpr int kIntAttributeType = attrs::userDefined + 1;
static constexpr int kStringAttributeType = attrs::userDefined + 100;

class StunMessageParser:
    public ::testing::Test
{
protected:
    void prepareMessageWithFingerprint()
    {
        prepareRandomMessage();
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
        m_testMessage.eraseAllAttributes();
    }

    void prepareSerializedMessageWithDuplicateAttributes()
    {
        prepareRandomMessage();
        serializeMessage();

        // Copying attributes.
        m_bufferToParse.append(m_bufferToParse.substr(MessageParser::kMessageHeaderSize));

        // Adjusting message length.
        std::uint16_t newMessageLength = htons(m_bufferToParse.size() - MessageParser::kMessageHeaderSize);
        memcpy(m_bufferToParse.data() + 0x02, &newMessageLength, sizeof(newMessageLength));
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
        int pos = rand() % (m_bufferToParse.size() - 8);
        // Making sure the value is actually changed.
        m_bufferToParse[pos] += 1 + (rand() % 247);
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

    void andParsedMessageIsExpected()
    {
        // The fingerprint is validated by the parser.
        m_testMessage.eraseAttribute(attrs::fingerPrint);
        m_parsedMessage.eraseAttribute(attrs::fingerPrint);

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

protected:
    Message m_testMessage;
    nx::Buffer m_bufferToParse;
    Message m_parsedMessage;

private:
    MessageParser m_parser;
    nx::network::server::ParserState m_prevParseResult = nx::network::server::ParserState::init;
    bool m_alwaysAddFingerprint = false;

    void assertEqual(const Message& one, const Message& two)
    {
        assertEqual(one.header, two.header);
        assertAttrsEqual(one, two);
    }

    void assertEqual(const Header& one, const Header& two)
    {
        ASSERT_EQ(one.messageClass, two.messageClass);
        ASSERT_EQ(one.method, two.method);
        ASSERT_EQ(one.transactionId, two.transactionId);
    }

    void assertAttrsEqual(const Message& one, const Message& two)
    {
        ASSERT_EQ(one.attributeCount(), two.attributeCount());

        one.forEachAttribute(
            [&two](const attrs::Attribute* oneAttr)
            {
                const attrs::Attribute* twoAttr = two.getAttribute<attrs::Attribute>(oneAttr->getType());
                ASSERT_NE(nullptr, twoAttr);
                if (oneAttr->getType() >= kIntAttributeType)
                {
                    ASSERT_EQ(
                        static_cast<const attrs::IntAttribute*>(oneAttr)->value(),
                        static_cast<const attrs::IntAttribute*>(twoAttr)->value());
                }
                else
                {
                    ASSERT_EQ(
                        static_cast<const attrs::Unknown*>(oneAttr)->getBuffer(),
                        static_cast<const attrs::Unknown*>(twoAttr)->getBuffer());
                }
            });
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

TEST_F(StunMessageParser, message_with_wrong_fingerprint_causes_parse_error)
{
    // Note that the message may be corrupted in such a way that it becomes a valid message with
    // no FINGERPRINT. So, fingerprint presence check is required.
    parser().setFingerprintRequired(true);

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

//-------------------------------------------------------------------------------------------------

class StunMessageIntegrity:
    public StunMessageParser
{
public:
    StunMessageIntegrity():
        m_userName("aaabbbcc"),
        m_userPassword("bbb")
    {
    }

protected:
    static constexpr int kMessageIntegrityLength = 20;

    void givenMessage()
    {
        nx::Buffer serialized;
        std::tie(m_testMessage, serialized) = prepareMessageWithIntegrity();
    }

    void givenSerializedMessageWithIntegrity()
    {
        std::tie(m_testMessage, m_bufferToParse) = prepareMessageWithIntegrity();
        addIntegrity();
    }

    void givenSerializedMessageWithLegacyIntegrity()
    {
        givenSerializedMessageWithIntegrity();

        // Replacing integrity (the last attribute) with zeros.
        m_bufferToParse.replace(
            m_bufferToParse.size() - kMessageIntegrityLength,
            kMessageIntegrityLength,
            nx::Buffer(kMessageIntegrityLength, '\0'));

        // Recalculating integrity taking into account zeroed MESSAGE-INTEGRITY attribute.
        auto integrity = nx::utils::auth::hmacSha1(m_userPassword, m_bufferToParse);
        m_testMessage.newAttribute<attrs::MessageIntegrity>(integrity);

        m_bufferToParse.replace(
            m_bufferToParse.size() - kMessageIntegrityLength,
            kMessageIntegrityLength,
            integrity);
    }

    void givenMessageWithIntegrity()
    {
        std::tie(m_testMessage, m_expectedSerializedMessage) = prepareMessageWithIntegrity();
        addIntegrity();
    }

    void addIntegrity()
    {
        m_testMessage.insertIntegrity(m_userName, m_userPassword);
    }

    void corruptMessageIntegrityAttribute()
    {
        std::size_t pos = findMessageIntegrity(m_bufferToParse);
        ASSERT_NE(0U, pos);

        ++m_bufferToParse[pos + 6];
    }

    void assertParsedMessageIntegrityCheckPasses(MessageIntegrityOptions options = {})
    {
        ASSERT_TRUE(m_parsedMessage.verifyIntegrity(m_userName, m_userPassword, options));
    }

    void useVerificationKey(const std::string& key)
    {
        m_userPassword = key;
    }

    void assertParsedMessageIntegrityCheckFails(MessageIntegrityOptions options = {})
    {
        ASSERT_FALSE(m_parsedMessage.verifyIntegrity(m_userName, m_userPassword, options));
    }

    void assertMessageContainsValidIntegrity()
    {
        auto msgIntegrityAttr = m_testMessage.getAttribute<attrs::MessageIntegrity>();
        ASSERT_NE(nullptr, msgIntegrityAttr);
        ASSERT_EQ(m_lastCalculatedIntegrity, msgIntegrityAttr->getBuffer());
    }

    void assertMessageSerializedCorrectly()
    {
        ASSERT_EQ(m_expectedSerializedMessage, m_bufferToParse);
    }

private:
    // Returns tuple<message, serialized message>.
    std::tuple<Message, nx::Buffer> prepareMessageWithIntegrity()
    {
        static constexpr char kDefaultTid[] = "012345670123456789ABCDEF";

        Message msg(Header(MessageClass::indication, MethodType::userMethod + 1));
        msg.header.transactionId = nx::utils::fromHex(kDefaultTid);
        const std::string nonce = "nonce888";
        msg.newAttribute<attrs::Nonce>(nonce);
        msg.newAttribute<attrs::UserName>(m_userName);
        msg.newAttribute<attrs::Unknown>(kStringAttributeType + 1, "ua1val");
        msg.newAttribute<attrs::Unknown>(kStringAttributeType + 2, "ua2val");

        EXPECT_EQ(8, m_userName.size()) << "The following code assumes that m_userName.size() is 8";

        nx::Buffer serializedMsg = nx::utils::fromHex(
            Buffer("00B1" "0048" "2112A442")        // indication 3, length=72 (attrs), magic cookie
            + kDefaultTid
            + Buffer("0015" "0008") + nx::utils::toUpper(nx::utils::toHex(nonce))
            + Buffer("0006" "0008") + nx::utils::toUpper(nx::utils::toHex(m_userName))
            + Buffer("E065" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua1val"))) + Buffer("0000")
            + Buffer("E066" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua2val"))) + Buffer("0000")
        );

        // Calculating integrity without MESSAGE-INTEGRITY attribute but with its length reflected in the message header.
        m_lastCalculatedIntegrity = nx::utils::auth::hmacSha1(m_userPassword, serializedMsg);
        serializedMsg += nx::utils::fromHex(Buffer("0008" "0014")) + m_lastCalculatedIntegrity;

        return std::make_tuple(msg, serializedMsg);
    }

private:
    std::string m_userName;
    std::string m_userPassword;
    nx::Buffer m_expectedSerializedMessage;
    nx::Buffer m_lastCalculatedIntegrity;
};

TEST_F(StunMessageIntegrity, calculated_integrity_is_valid)
{
    givenMessage();
    addIntegrity();

    assertMessageContainsValidIntegrity();
}

TEST_F(StunMessageIntegrity, message_fingerprint_is_ignored_when_adding_integrity)
{
    givenMessage();
    addFingerprint();
    addIntegrity();

    assertMessageContainsValidIntegrity();
}

TEST_F(StunMessageIntegrity, integrity_is_serialized)
{
    givenMessageWithIntegrity();
    serializeMessage();
    assertMessageSerializedCorrectly();
}

TEST_F(StunMessageIntegrity, correct_integrity_is_verified)
{
    givenSerializedMessageWithIntegrity();
    assertMessageCanBeParsed();

    assertParsedMessageIntegrityCheckPasses();
}

TEST_F(StunMessageIntegrity, integrity_verification_fails_with_wrong_key)
{
    givenSerializedMessageWithIntegrity();
    assertMessageCanBeParsed();

    useVerificationKey("wrong_key");
    assertParsedMessageIntegrityCheckFails();
}

TEST_F(StunMessageIntegrity, corrupted_integrity_is_not_verified)
{
    givenSerializedMessageWithIntegrity();
    corruptMessageIntegrityAttribute();
    assertMessageCanBeParsed();

    assertParsedMessageIntegrityCheckFails();
}

// The STUN implementation had the following bug from the very beginning:
// it added zeroed MESSAGE-INTEGRITY when calculating the hmac.
// This is wrong (see https://datatracker.ietf.org/doc/html/rfc8489#section-14.5),
// but the backward compatibility between VMS and Cloud has to be preserved.
// So, ths functionality has been kept as legacy.
TEST_F(StunMessageIntegrity, legacy_integrity_verification)
{
    givenSerializedMessageWithLegacyIntegrity();

    assertMessageCanBeParsed();
    assertParsedMessageIntegrityCheckPasses({.legacyMode = true});
}

TEST_F(StunMessageIntegrity, fingerprint_and_integrity)
{
    givenMessage();
    addFingerprint();
    addIntegrity();

    serializeMessage();

    assertMessageCanBeParsed();
    assertParsedMessageIntegrityCheckPasses();
}

} // namespace nx::network::stun::test
