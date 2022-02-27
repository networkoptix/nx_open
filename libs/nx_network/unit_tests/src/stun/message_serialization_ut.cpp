// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/cloud/data/bind_data.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/stun/message_parser.h>
#include <nx/utils/auth/utils.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

static const Buffer DEFAULT_TID("012345670123456789ABCDEF");

/** Parses @param message by @param partSize chunks in @param @parser */
static void partialParse(MessageParser& parser, const Buffer& message, size_t partSize)
{
    size_t parsedNow;
    size_t parsedTotal = 0;
    for (; parsedTotal < message.size() - partSize; parsedTotal += partSize)
    {
        const Buffer part(message.data() + parsedTotal, partSize);
        ASSERT_EQ(parser.parse(part, &parsedNow), nx::network::server::ParserState::readingMessage);
        ASSERT_EQ(parsedNow, partSize);
    }

    const size_t lastPartSize = message.size() - parsedTotal;
    const Buffer lastPart(message.data() + parsedTotal, lastPartSize);
    ASSERT_EQ(parser.parse(lastPart, &parsedNow), nx::network::server::ParserState::done);
    ASSERT_EQ(parsedNow, lastPartSize);
}

TEST(StunMessageSerialization, BindingRequest)
{
    Message request(Header(MessageClass::request, MethodType::bindingMethod));
    request.header.transactionId = nx::utils::fromHex(DEFAULT_TID);

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(&request);
    ASSERT_EQ(serializer.serialize(&serializedMessage, &serializedSize),
        nx::network::server::SerializerState::done);

    static const Buffer MESSAGE = Buffer(
        "0001" "0000" "2112A442"    // request: binding, length, magic cookie
    ) + DEFAULT_TID;                // transaction id (rev)
    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    EXPECT_EQ(0, nx::utils::stricmp(MESSAGE, nx::utils::toHex(serializedMessage)));

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 1); // parse by 1 byte chunks

    ASSERT_EQ(parsed.header.messageClass, MessageClass::request);
    ASSERT_EQ(parsed.header.method, MethodType::bindingMethod);
    ASSERT_EQ(0, nx::utils::stricmp(nx::utils::toHex(parsed.header.transactionId), DEFAULT_TID));
    ASSERT_EQ(parsed.attributes.size(), (size_t)0);
}

TEST(StunMessageSerialization, BindingResponse)
{
    Message response(Header(MessageClass::successResponse, MethodType::bindingMethod));
    response.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
    response.newAttribute< attrs::XorMappedAddress >(0x1234, 0x12345678);

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(&response);
    ASSERT_EQ(serializer.serialize(&serializedMessage, &serializedSize),
        nx::network::server::SerializerState::done);

    static const Buffer MESSAGE = Buffer(
        "0101" "000C" "2112A442"    // response: binding, length=12, magic cookie
    ) + DEFAULT_TID + Buffer(
        "0020" "0008" "0001"        // xor-mapped-address, ipv4
        "3326" "3326F23A"           // xor port, xor ip
    );
    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    EXPECT_EQ(0, nx::utils::stricmp(MESSAGE, nx::utils::toHex(serializedMessage)));

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 2); // parse by 2 byte chanks

    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    ASSERT_EQ(parsed.header.messageClass, MessageClass::successResponse);
    ASSERT_EQ(parsed.header.method, MethodType::bindingMethod);
    ASSERT_EQ(0, nx::utils::stricmp(nx::utils::toHex(parsed.header.transactionId), DEFAULT_TID));
    ASSERT_EQ(parsed.attributes.size(), (size_t)1);

    const auto address = parsed.getAttribute< attrs::XorMappedAddress >();
    ASSERT_NE(address, nullptr);
    ASSERT_EQ(address->address.ipv4, (size_t)0x12345678);
}

TEST(StunMessageSerialization, BindingError)
{
    Message response(Header(MessageClass::errorResponse, MethodType::bindingMethod));
    response.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
    response.newAttribute< attrs::ErrorCode >(401, "Unauthorized");

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(&response);
    ASSERT_EQ(serializer.serialize(&serializedMessage, &serializedSize),
        nx::network::server::SerializerState::done);

    static const auto MESSAGE = Buffer(
        "0111" "0014" "2112A442"    // reponse: binding, lenght=20, magic cookie
    ) + DEFAULT_TID + Buffer(
        "0009" "0010" "00000401"    // error code 0x3 0x77
    ) + nx::utils::toHex(Buffer("Unauthorized"));

    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    EXPECT_EQ(0, nx::utils::stricmp(MESSAGE, nx::utils::toHex(serializedMessage)));

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 3); // parse by 3 byte chanks

    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    ASSERT_EQ(parsed.header.messageClass, MessageClass::errorResponse);
    ASSERT_EQ(parsed.header.method, MethodType::bindingMethod);
    ASSERT_EQ(0, nx::utils::stricmp(nx::utils::toHex(parsed.header.transactionId), DEFAULT_TID));
    ASSERT_EQ(parsed.attributes.size(), (size_t)1);

    const auto error = parsed.getAttribute< attrs::ErrorCode >();
    ASSERT_NE(error, nullptr);
    ASSERT_EQ(error->getClass(), 4);
    ASSERT_EQ(error->getNumber(), 1);
    ASSERT_EQ(error->getString(), "Unauthorized");
}

TEST(StunMessageSerialization, CustomIndication)
{
    Message response(Header(MessageClass::indication, MethodType::userMethod + 1));
    response.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
    response.newAttribute< attrs::Unknown >(0x9001, "ua1val");
    response.newAttribute< attrs::Unknown >(0x9002, "ua2val");

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(&response);
    ASSERT_EQ(serializer.serialize(&serializedMessage, &serializedSize),
        nx::network::server::SerializerState::done);

    static const auto MESSAGE = Buffer(
        "00B1" "0018" "2112A442"        // indication 3, lenght=12, magic cookie
    ) + DEFAULT_TID
        + Buffer("9001" "0006") + nx::utils::toHex(Buffer("ua1val")) + Buffer("0000")
        + Buffer("9002" "0006") + nx::utils::toHex(Buffer("ua2val")) + Buffer("0000");

    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    EXPECT_EQ(0, nx::utils::stricmp(MESSAGE, nx::utils::toHex(serializedMessage)));

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 4); // parse by 4 byte chanks

    ASSERT_EQ((size_t)serializedMessage.size(), serializedSize);
    ASSERT_EQ(parsed.header.messageClass, MessageClass::indication);
    ASSERT_EQ(parsed.header.method, MethodType::userMethod + 1);
    ASSERT_EQ(0, nx::utils::stricmp(nx::utils::toHex(parsed.header.transactionId), DEFAULT_TID));
    ASSERT_EQ(parsed.attributes.size(), 2U);

    const auto attr1 = parsed.getAttribute< attrs::Unknown >(0x9001);
    ASSERT_NE(attr1, nullptr);
    ASSERT_EQ(attr1->getString(), Buffer("ua1val"));

    const auto attr2 = parsed.getAttribute< attrs::Unknown >(0x9002);
    ASSERT_NE(attr2, nullptr);
    ASSERT_EQ(attr2->getString(), Buffer("ua2val"));
}

TEST(StunMessageSerialization, serialization2)
{
    Message message(Header(MessageClass::request, MethodType::bindingMethod));

    const std::string userName("sdfno234sdf");
    const std::string nonce("kdfgjn234df");

    //message.newAttribute<stun::extension::attrs::SystemId>(userName);
    const std::string testData("sdfr234dfg");
    message.newAttribute<stun::extension::attrs::ServerId>(testData);

    size_t bytesRead = 0;

    //serializing message
    Buffer buffer;
    {
        buffer.reserve(100);
        MessageSerializer serializer;
        serializer.setMessage(&message);
        ASSERT_EQ(nx::network::server::SerializerState::done, serializer.serialize(&buffer, &bytesRead));
    }

    //deserializing message and checking that it equals to the one before serialization
    MessageParser parser;
    Message checkMessage;
    parser.setMessage(&checkMessage);
    ASSERT_EQ(nx::network::server::ParserState::done, parser.parse(buffer, &bytesRead));
    const auto attr = checkMessage.getAttribute<stun::extension::attrs::ServerId>();
    ASSERT_EQ(testData, attr->getBuffer());

    Buffer buffer1;
    buffer1.reserve(100);
    MessageSerializer serializer1;
    serializer1.setMessage(&checkMessage);
    ASSERT_EQ(nx::network::server::SerializerState::done, serializer1.serialize(&buffer1, &bytesRead));
    ASSERT_EQ(buffer1, buffer);
}

TEST(StunMessageSerialization, serialization3)
{
    stun::Message response(stun::Header(
        stun::MessageClass::errorResponse,
        stun::MethodType::bindingMethod,
        "abra-kadabra"));

    // TODO: verify with RFC
    response.newAttribute< stun::attrs::ErrorCode >(
        404, "Method is not supported");    //TODO #akolesnikov replace 404 with constant

    Buffer serializedMessage;
    serializedMessage.reserve(100);
    MessageSerializer serializer1;
    serializer1.setMessage(&response);
    size_t bytesWritten = 0;
    ASSERT_EQ(
        nx::network::server::SerializerState::done,
        serializer1.serialize(&serializedMessage, &bytesWritten));


    MessageParser parser;
    Message checkMessage;
    parser.setMessage(&checkMessage);
    size_t bytesRead = 0;
    ASSERT_EQ(
        nx::network::server::ParserState::done,
        parser.parse(serializedMessage, &bytesRead));
    /*const auto attr =*/ checkMessage.getAttribute<stun::attrs::ErrorCode>();
    //ASSERT_EQ(testData, attr->getBuffer());
}

TEST(StunMessageSerialization, Authentification)
{
    std::string user("user"), key("key");

    Message request(Header(MessageClass::request, MethodType::bindingMethod));
    //const nx::Buffer testData("sdfr234dfg");
    //request.newAttribute<stun::extension::attrs::ServerId>(testData);
    request.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
    request.insertIntegrity(user, key);

    size_t serializedSize = 0;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(&request);
    ASSERT_EQ(
        serializer.serialize(&serializedMessage, &serializedSize),
        nx::network::server::SerializerState::done);

    const auto miAttr = request.getAttribute< attrs::MessageIntegrity >();
    const auto nonceAttr = request.getAttribute< attrs::Nonce >();
    ASSERT_TRUE(miAttr);
    ASSERT_TRUE(nonceAttr);

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 5); // parse by 1 byte chunks

    ASSERT_TRUE(parsed.verifyIntegrity(user, key));
    ASSERT_EQ(parsed.header.messageClass, MessageClass::request);
    ASSERT_EQ(parsed.header.method, MethodType::bindingMethod);
    ASSERT_EQ(0, nx::utils::stricmp(nx::utils::toHex(parsed.header.transactionId), DEFAULT_TID));
    ASSERT_EQ(parsed.attributes.size(), 3U);
}

//-------------------------------------------------------------------------------------------------

class StunMessageSerializer:
    public ::testing::Test
{
public:
    StunMessageSerializer()
    {
        m_message = Message(Header(MessageClass::request, MethodType::bindingMethod));
        m_message.header.transactionId =
            nx::utils::generateRandomName(Header::TRANSACTION_ID_SIZE);

        m_username = "username"; // size fixed to 8 bytes for the test simplicity.
        m_key = "key";

        m_serializer.setAlwaysAddFingerprint(false);
    }

protected:
    void givenEmptyOutputBuffer()
    {
        m_outputBuffer = nx::Buffer();
    }

    std::uint32_t /*fingerprint*/ givenMessageWithFingerprint()
    {
        m_message = Message(Header(MessageClass::indication, MethodType::userMethod + 1));
        m_message.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
        m_message.newAttribute<attrs::Unknown>(0x9001, "ua1val");
        m_message.newAttribute<attrs::Unknown>(0x9002, "ua2val");

        const Buffer kMessageForFingerprint = nx::utils::fromHex(
            Buffer("00B1" "0020" "2112A442"        // indication 3, length=24 (attrs) + 8 (fingerprint), magic cookie
            ) + DEFAULT_TID
            + Buffer("9001" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua1val"))) + Buffer("0000")
            + Buffer("9002" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua2val"))) + Buffer("0000"));

        boost::crc_32_type crc32;
        crc32.process_bytes(kMessageForFingerprint.data(), kMessageForFingerprint.size());
        return crc32.checksum() ^ 0x5354554e;
    }

    nx::Buffer givenMessageWithIntegrity()
    {
        m_message = Message(Header(MessageClass::indication, MethodType::userMethod + 1));
        m_message.header.transactionId = nx::utils::fromHex(DEFAULT_TID);
        m_message.newAttribute<attrs::Unknown>(0x9001, "ua1val");
        m_message.newAttribute<attrs::Unknown>(0x9002, "ua2val");
        m_message.newAttribute<attrs::UserName>(m_username);
        const std::string nonce = "nonce888";
        m_message.newAttribute<attrs::Nonce>(nonce);
        m_message.insertIntegrity(m_username, m_key);

        // The STUN implementation has the following bug from the very beginning:
        // it adds zeroed MESSAGE-INTEGRITY when calculating the hmac.
        // This is wrong (see https://datatracker.ietf.org/doc/html/rfc8489#section-14.5),
        // but the backward compatibility between VMS and Cloud has to be preserved.
        // So, not fixing this bug.

        const Buffer messageWithIntegrity = nx::utils::fromHex(
            Buffer("00B1" "0048" "2112A442")        // indication 3, length=72 (attrs), magic cookie
            + DEFAULT_TID
            + Buffer("0006" "0008") + nx::utils::toUpper(nx::utils::toHex(m_username))
            + Buffer("0015" "0008") + nx::utils::toUpper(nx::utils::toHex(nonce))
            + Buffer("9001" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua1val"))) + Buffer("0000")
            + Buffer("9002" "0006") + nx::utils::toUpper(nx::utils::toHex(Buffer("ua2val"))) + Buffer("0000")
            + Buffer("0008" "0014") + nx::utils::toUpper(nx::utils::toHex(nx::Buffer(20, '\0')))
        );

        return nx::utils::auth::hmacSha1(m_key, messageWithIntegrity);
    }

    std::uint32_t addFingerprint()
    {
        m_message.attributes.erase(attrs::fingerPrint);

        MessageSerializer serializer;
        serializer.setAlwaysAddFingerprint(false);
        nx::Buffer msgWithoutFingerprint = serializer.serialized(m_message);

        m_message.newAttribute<attrs::FingerPrint>();

        // Fixing length.
        std::uint16_t len = 0;
        memcpy(&len, msgWithoutFingerprint.data() + 2, sizeof(std::uint16_t));
        len = ntohs(len);
        len += 8; // FINGERPRINT length.
        len = htons(len);
        memcpy(msgWithoutFingerprint.data() + 2, &len, sizeof(std::uint16_t));

        boost::crc_32_type crc32;
        crc32.process_bytes(msgWithoutFingerprint.data(), msgWithoutFingerprint.size());
        return crc32.checksum() ^ 0x5354554e;
    }

    void whenSerializeMessage()
    {
        m_serializer.setMessage(&m_message);
        if (m_alwaysAddFingerprint)
            m_serializer.setAlwaysAddFingerprint(*m_alwaysAddFingerprint);
        m_prevSerializationResult = m_serializer.serialize(&m_outputBuffer, &m_bytesWritten);
    }

    void thenMessageSerializedSuccessfully()
    {
        ASSERT_EQ(
            nx::network::server::SerializerState::done,
            m_prevSerializationResult);
    }

    void andSerializedMessageEndsWith(const nx::Buffer& suffix)
    {
        ASSERT_TRUE(m_outputBuffer.ends_with(suffix));
    }

    void andSerializedMessageContains(const nx::Buffer& expectedSubstr)
    {
        ASSERT_TRUE(m_outputBuffer.contains(expectedSubstr));
    }

    void andSerializedMessageContainsFingerprint(std::uint32_t expected)
    {
        // Last 4 bytes MUST be fingerprint value.
        const auto expectedFingerprintBigEndian = htonl(expected);
        andSerializedMessageEndsWith(nx::Buffer(
            reinterpret_cast<const char*>(&expectedFingerprintBigEndian),
            sizeof(expectedFingerprintBigEndian)));
    }

    void setAlwaysAddFingerprint(bool val)
    {
        m_alwaysAddFingerprint = val;
    }

private:
    Message m_message;
    MessageSerializer m_serializer;
    nx::Buffer m_outputBuffer;
    std::size_t m_bytesWritten = 0;
    nx::network::server::SerializerState m_prevSerializationResult =
        nx::network::server::SerializerState::done;
    std::optional<bool> m_alwaysAddFingerprint;
    std::string m_username;
    std::string m_key;
};

TEST_F(StunMessageSerializer, serializer_accepts_empty_output_buffer)
{
    givenEmptyOutputBuffer();
    whenSerializeMessage();
    thenMessageSerializedSuccessfully();
}

TEST_F(StunMessageSerializer, fingerprint_is_added_to_every_message_if_enabled)
{
    const auto expectedFingerprint = givenMessageWithFingerprint();

    setAlwaysAddFingerprint(true);
    whenSerializeMessage();

    thenMessageSerializedSuccessfully();

    andSerializedMessageContainsFingerprint(expectedFingerprint);
}

TEST_F(StunMessageSerializer, valid_message_integrity_is_added)
{
    const auto expectedIntegrity = givenMessageWithIntegrity();

    whenSerializeMessage();

    thenMessageSerializedSuccessfully();
    andSerializedMessageEndsWith(expectedIntegrity);
}

TEST_F(StunMessageSerializer, message_with_integrity_and_fingerprint)
{
    const auto expectedIntegrity = givenMessageWithIntegrity();
    const auto expectedFingerprint = addFingerprint();

    whenSerializeMessage();

    thenMessageSerializedSuccessfully();
    andSerializedMessageContainsFingerprint(expectedFingerprint);
    andSerializedMessageContains(expectedIntegrity);
}

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
