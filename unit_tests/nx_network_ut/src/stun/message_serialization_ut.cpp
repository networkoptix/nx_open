#include <gtest/gtest.h>

#include <nx/network/cloud/data/bind_data.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/stun/message_parser.h>
#include <nx/utils/random.h>

namespace nx {
namespace stun {
namespace test {

static const Buffer DEFAULT_TID( "012345670123456789ABCDEF" );

/** Parses @param message by @param partSize chunks in @param @parser */
static void partialParse( MessageParser& parser, const Buffer& message, size_t partSize )
{
    size_t parsedNow;
    size_t parsedTotal = 0;
    for ( ; parsedTotal < message.size() - partSize; parsedTotal += partSize )
    {
        const Buffer part( message.data() + parsedTotal, partSize );
        ASSERT_EQ( parser.parse(part, &parsedNow), nx_api::ParserState::inProgress );
        ASSERT_EQ( parsedNow, partSize );
    }

    const size_t lastPartSize = message.size() - parsedTotal;
    const Buffer lastPart(message.data() + parsedTotal, lastPartSize);
    ASSERT_EQ( parser.parse(lastPart, &parsedNow), nx_api::ParserState::done );
    ASSERT_EQ( parsedNow, lastPartSize );
}

TEST( StunMessageSerialization, BindingRequest )
{
    Message request( Header( MessageClass::request, MethodType::bindingMethod ) );
    request.header.transactionId = Buffer::fromHex( DEFAULT_TID );

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( &request );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const Buffer MESSAGE = Buffer(
            "0001" "0000" "2112A442"    // request: binding, length, magic cookie
        ) + DEFAULT_TID;                // transaction id (rev)
    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    EXPECT_EQ( MESSAGE, serializedMessage.toHex().toUpper());

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 1); // parse by 1 byte chunks

    ASSERT_EQ( parsed.header.messageClass, MessageClass::request );
    ASSERT_EQ( parsed.header.method, MethodType::bindingMethod );
    ASSERT_EQ( parsed.header.transactionId.toHex().toUpper(), DEFAULT_TID );
    ASSERT_EQ( parsed.attributes.size(), (size_t)0 );
}

TEST( StunMessageSerialization, BindingResponse )
{
    Message response( Header( MessageClass::successResponse, MethodType::bindingMethod ) );
    response.header.transactionId = Buffer::fromHex( DEFAULT_TID );
    response.newAttribute< attrs::XorMappedAddress >( 0x1234, 0x12345678 );

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( &response );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const Buffer MESSAGE = Buffer(
            "0101" "000C" "2112A442"    // reponse: binding, lenght=12, magic cookie
         ) + DEFAULT_TID + Buffer(
            "0020" "0008" "0001"        // xor-mapped-address, ipv4
            "3326" "3326F23A"           // xor port, xor ip
        );
    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    EXPECT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 2); // parse by 2 byte chanks

    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    ASSERT_EQ( parsed.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( parsed.header.method, MethodType::bindingMethod );
    ASSERT_EQ( parsed.header.transactionId.toHex().toUpper(), DEFAULT_TID );
    ASSERT_EQ( parsed.attributes.size(), (size_t)1 );

    const auto address = parsed.getAttribute< attrs::XorMappedAddress >();
    ASSERT_NE( address, nullptr );
    ASSERT_EQ( address->address.ipv4, (size_t)0x12345678 );
}

TEST( StunMessageSerialization, BindingError )
{
    Message response( Header( MessageClass::errorResponse, MethodType::bindingMethod ) );
    response.header.transactionId = Buffer::fromHex( DEFAULT_TID );
    response.newAttribute< attrs::ErrorCode >( 401, "Unauthorized" );

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( &response );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const auto MESSAGE = Buffer(
            "0111" "0014" "2112A442"    // reponse: binding, lenght=20, magic cookie
        ) + DEFAULT_TID + Buffer(
            "0009" "0010" "00000401"    // error code 0x3 0x77
        ) + Buffer( "Unauthorized" ).toHex().toUpper();

    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    EXPECT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );

    Message parsed;
    MessageParser parser;
    parser.setMessage( &parsed );
    partialParse( parser, serializedMessage, 3 ); // parse by 3 byte chanks

    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    ASSERT_EQ( parsed.header.messageClass, MessageClass::errorResponse );
    ASSERT_EQ( parsed.header.method, MethodType::bindingMethod );
    ASSERT_EQ( parsed.header.transactionId.toHex().toUpper(), DEFAULT_TID );
    ASSERT_EQ( parsed.attributes.size(), (size_t)1 );

    const auto error = parsed.getAttribute< attrs::ErrorCode >();
    ASSERT_NE( error, nullptr );
    ASSERT_EQ( error->getClass(), 4 );
    ASSERT_EQ( error->getNumber(), 1 );
    ASSERT_EQ( error->getString(), String( "Unauthorized" ) );
}

TEST( StunMessageSerialization, CustomIndication )
{
    Message response( Header( MessageClass::indication, MethodType::userMethod + 1 ) );
    response.header.transactionId = Buffer::fromHex( DEFAULT_TID );
    response.newAttribute< attrs::Unknown >( 0x9001, stringToBuffer( "ua1val" ) );
    response.newAttribute< attrs::Unknown >( 0x9002, stringToBuffer( "ua2val" ) );

    size_t serializedSize;
    Buffer serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( &response );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const auto MESSAGE = Buffer(
            "00B1" "0018" "2112A442"        // indication 3, lenght=12, magic cookie
        ) + DEFAULT_TID
          + Buffer( "9001" "0006" ) + Buffer( "ua1val" ).toHex().toUpper() + Buffer( "0000" )
          + Buffer( "9002" "0006" ) + Buffer( "ua2val" ).toHex().toUpper() + Buffer( "0000" );

    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    EXPECT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );

    Message parsed;
    MessageParser parser;
    parser.setMessage( &parsed );
    partialParse( parser, serializedMessage, 4 ); // parse by 4 byte chanks

    ASSERT_EQ( (size_t)serializedMessage.size(), serializedSize );
    ASSERT_EQ( parsed.header.messageClass, MessageClass::indication );
    ASSERT_EQ( parsed.header.method, MethodType::userMethod + 1 );
    ASSERT_EQ( parsed.header.transactionId.toHex().toUpper(), DEFAULT_TID );
    ASSERT_EQ( parsed.attributes.size(), 2U );

    const auto attr1 = parsed.getAttribute< attrs::Unknown >( 0x9001 );
    ASSERT_NE( attr1, nullptr );
    ASSERT_EQ( attr1->getString(), Buffer( "ua1val" ) );

    const auto attr2 = parsed.getAttribute< attrs::Unknown >( 0x9002 );
    ASSERT_NE( attr2, nullptr );
    ASSERT_EQ( attr2->getString(), Buffer( "ua2val" ) );
}

TEST(StunMessageSerialization, serialization2)
{
    Message message(Header(MessageClass::request, MethodType::bindingMethod));

    const nx::String userName("sdfno234sdf");
    const nx::String nonce("kdfgjn234df");

    //message.newAttribute<stun::extension::attrs::SystemId>(userName);
    const nx::Buffer testData("sdfr234dfg");
    message.newAttribute<stun::extension::attrs::ServerId>(testData);

    //message.newAttribute< attrs::UserName >(userName);
    //message.newAttribute< attrs::Nonce >(nonce.toHex());
    //message.newAttribute< attrs::MessageIntegrity >(nx::Buffer(
    //    attrs::MessageIntegrity::SIZE, 0));

    size_t bytesRead = 0;

    //serializing message
    Buffer buffer;
    {
        buffer.reserve(100);
        MessageSerializer serializer;
        serializer.setMessage(&message);
        ASSERT_EQ(nx_api::SerializerState::done, serializer.serialize(&buffer, &bytesRead));
    }

    //deserializing message and checking that it equals to the one before serialization
    MessageParser parser;
    Message checkMessage;
    parser.setMessage(&checkMessage);
    ASSERT_EQ(nx_api::ParserState::done, parser.parse(buffer, &bytesRead));
    const auto attr = checkMessage.getAttribute<stun::extension::attrs::ServerId>();
    ASSERT_EQ(testData, attr->getBuffer());

    Buffer buffer1;
    buffer1.reserve(100);
    MessageSerializer serializer1;
    serializer1.setMessage(&checkMessage);
    ASSERT_EQ(nx_api::SerializerState::done, serializer1.serialize(&buffer1, &bytesRead));
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
        404, "Method is not supported");    //TODO #ak replace 404 with constant

    Buffer serializedMessage;
    serializedMessage.reserve(100);
    MessageSerializer serializer1;
    serializer1.setMessage(&response);
    size_t bytesWritten = 0;
    ASSERT_EQ(
        nx_api::SerializerState::done,
        serializer1.serialize(&serializedMessage, &bytesWritten));


    MessageParser parser;
    Message checkMessage;
    parser.setMessage(&checkMessage);
    size_t bytesRead = 0;
    ASSERT_EQ(
        nx_api::ParserState::done,
        parser.parse(serializedMessage, &bytesRead));
    /*const auto attr =*/ checkMessage.getAttribute<stun::attrs::ErrorCode>();
    //ASSERT_EQ(testData, attr->getBuffer());
}

TEST( StunMessageSerialization, Authentification )
{
    String user( "user" ), key( "key" );

    Message request( Header( MessageClass::request, MethodType::bindingMethod ) );
    //const nx::Buffer testData("sdfr234dfg");
    //request.newAttribute<stun::extension::attrs::ServerId>(testData);
    request.header.transactionId = Buffer::fromHex( DEFAULT_TID );
    request.insertIntegrity( user, key );

    size_t serializedSize = 0;
    Buffer serializedMessage;
    serializedMessage.reserve(100);

    MessageSerializer serializer;
    serializer.setMessage(&request);
    ASSERT_EQ(
        serializer.serialize(&serializedMessage, &serializedSize),
        nx_api::SerializerState::done);

    const auto miAttr = request.getAttribute< attrs::MessageIntegrity >();
    const auto nonceAttr = request.getAttribute< attrs::Nonce >();
    ASSERT_TRUE(miAttr);
    ASSERT_TRUE(nonceAttr);

    //const Buffer MESSAGE =
    //    Buffer("0001" "003C" "2112A442") + // request: binding, length, magic cookie
    //    DEFAULT_TID +                        // transaction id (rev)
    //    Buffer("0006" "0004") + user.toHex().toUpper() +
    //    Buffer("0015" "0014") + nonceAttr->getBuffer().toHex().toUpper() +
    //    Buffer("0008" "0014") + miAttr->getBuffer().toHex().toUpper();
    //ASSERT_EQ(serializedMessage.size(), serializedSize);
    //EXPECT_EQ(MESSAGE, serializedMessage.toHex().toUpper());

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 5); // parse by 1 byte chunks

    ASSERT_TRUE(parsed.verifyIntegrity(user, key));
    ASSERT_EQ(parsed.header.messageClass, MessageClass::request);
    ASSERT_EQ(parsed.header.method, MethodType::bindingMethod);
    ASSERT_EQ(parsed.header.transactionId.toHex().toUpper(), DEFAULT_TID);
    ASSERT_EQ(parsed.attributes.size(), 3U);
}

//-------------------------------------------------------------------------------------------------

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
        ASSERT_EQ(nx_api::ParserState::done, m_prevParseResult);
    }

    void thenParseDidNotSucceed()
    {
        ASSERT_NE(nx_api::ParserState::done, m_prevParseResult);
    }

private:
    Message m_messageToParse;
    Message m_parsedMessage;
    MessageParser m_parser;
    nx::Buffer m_bufferToParse;
    nx_api::ParserState m_prevParseResult = nx_api::ParserState::init;

    void prepareCorrectStunPacket()
    {
        m_messageToParse = Message(Header(MessageClass::request, MethodType::bindingMethod));
        m_messageToParse.header.transactionId = Buffer::fromHex(DEFAULT_TID);
     
        MessageSerializer serializer;
        serializer.setMessage(&m_messageToParse);
        m_bufferToParse.clear();
        m_bufferToParse.reserve(16*1024);
        size_t serializedSize = 0;
        ASSERT_EQ(
            serializer.serialize(&m_bufferToParse, &serializedSize),
            nx_api::SerializerState::done);
    }
};

TEST_F(StunMessageParser, is_able_to_parse_after_junk_message)
{
    whenParseMultipleJunkPackets();
    thenParserIsAbleToParseCorrectPacket();
}

} // namespace test
} // namespase stun
} // namespase nx
