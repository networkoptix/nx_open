#include <gtest/gtest.h>

#include <utils/network/stun/stun_message_serializer.h>
#include <utils/network/stun/stun_message_parser.h>

namespace nx_stun {

/** Parses @param message by @param partSize chunks in @param @parser */
void partialParse(MessageParser& parser, const QByteArray& message, size_t partSize)
{
    size_t parsedNow;
    size_t parsedTotal = 0;
    for ( ; parsedTotal < message.size() - partSize; parsedTotal += partSize )
    {
        const QByteArray part( message.data() + parsedTotal, partSize );
        ASSERT_EQ( parser.parse(part, &parsedNow), nx_api::ParserState::inProgress );
        ASSERT_EQ( parsedNow, partSize );
    }

    const size_t lastPartSize = message.size() - parsedTotal;
    const QByteArray lastPart(message.data() + parsedTotal, lastPartSize);
    ASSERT_EQ( parser.parse(lastPart, &parsedNow), nx_api::ParserState::done );
    ASSERT_EQ( parsedNow, lastPartSize );
}

TEST( StunMessageSerialization, BindingRequest )
{
    Message request( Header( MessageClass::request, static_cast< int >( MethodType::binding ) ) );
    request.header.transactionID.set( 0x01234567, 0x0123456789ABCDEF );

    size_t serializedSize;
    QByteArray serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( std::move( request ) );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const QByteArray MESSAGE(
        "0001" "0000" "2112A442"    // request: binding, length, magic cookie
        "EFCDAB896745230167452301"  // transaction id (rev)
    );
    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( MESSAGE, serializedMessage.toHex().toUpper());

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 1); // parse by 1 byte chunks

    ASSERT_EQ( parsed.header.messageClass, MessageClass::request );
    ASSERT_EQ( parsed.header.method, static_cast< int >( MethodType::binding ) );
    ASSERT_EQ( parsed.attributes.size(), 0 );

    uint32_t highId;
    uint64_t lowId;
    parsed.header.transactionID.get( &highId, &lowId);
    ASSERT_EQ( highId, 0x01234567 );
    ASSERT_EQ( lowId, 0x0123456789ABCDEF );
}

TEST( StunMessageSerialization, BindingResponse )
{
    Message response( Header( MessageClass::successResponse,
                             static_cast< int >( MethodType::binding ) ) );
    response.header.transactionID.set( 0x01234567, 0x0123456789ABCDEF );
    response.addAttribute( std::make_unique< attr::XorMappedAddress >(
                              0x1234, 0x12345678 ) );

    size_t serializedSize;
    QByteArray serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( std::move( response ) );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const QByteArray MESSAGE(
        "0101" "000C" "2112A442"    // reponse: binding, lenght=12, magic cookie
        "EFCDAB896745230167452301"  // transaction id (rev)
        "0020" "0008" "0001"        // xor-mapped-address, ipv4
        "3326" "3326F23A"           // xor port, xor ip
    );
    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 2); // parse by 2 byte chanks

    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( parsed.header.messageClass, MessageClass::successResponse );
    ASSERT_EQ( parsed.header.method, static_cast< int >( MethodType::binding ) );
    ASSERT_EQ( parsed.attributes.size(), 1 );

    const auto attribute = parsed.attributes.find(attr::AttributeType::xorMappedAddress);
    ASSERT_NE( attribute, parsed.attributes.end() );

    const auto address = dynamic_cast<attr::XorMappedAddress*>(attribute->second.get());
    ASSERT_NE( address, nullptr );
    ASSERT_EQ( address->address.ipv4, 0x12345678 );
}

TEST( StunMessageSerialization, BindingError )
{
    Message response( Header( MessageClass::errorResponse,
                             static_cast< int >( MethodType::binding ) ) );
    response.header.transactionID.set( 0x01234567, 0x0123456789ABCDEF );
    response.addAttribute( std::make_unique< attr::ErrorDescription >(
                              401, "Unauthorized" ) );

    size_t serializedSize;
    QByteArray serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( std::move( response ) );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const QByteArray MESSAGE = QByteArray(
            "0111" "0014" "2112A442"    // reponse: binding, lenght=12, magic cookie
            "EFCDAB896745230167452301"  // transaction id (rev)
            "0009" "0010" "00000401"    // error code 0x3 0x77
        ) + QByteArray( "Unauthorized" ).toHex().toUpper();

    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );

    Message parsed;
    MessageParser parser;
    parser.setMessage(&parsed);
    partialParse(parser, serializedMessage, 4); // parse by 4 byte chanks

    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( parsed.header.messageClass, MessageClass::errorResponse );
    ASSERT_EQ( parsed.header.method, static_cast< int >( MethodType::binding ) );
    ASSERT_EQ( parsed.attributes.size(), 1 );

    const auto attribute = parsed.attributes.find(attr::AttributeType::errorCode);
    ASSERT_NE( attribute, parsed.attributes.end() );

    const auto error = dynamic_cast<attr::ErrorDescription*>(attribute->second.get());
    ASSERT_NE( error, nullptr );
    ASSERT_EQ( error->getClass(), 4 );
    ASSERT_EQ( error->getNumber(), 1 );
    ASSERT_EQ( error->reasonPhrase, std::string( "Unauthorized" ) );
}

TEST( StunMessageSerialization, CustomIndication )
{
    // TODO: implement

    /*
    Message response( Header( MessageClass::indication,
                             static_cast< int >( MethodType::userMethod ) + 1 ) );
    response.header.transactionID.set( 0x01234567, 0x0123456789ABCDEF );
    response.addAttribute( std::make_unique< attr::UnknownAttribute >( 1, "ua1v" ) );
    response.addAttribute( std::make_unique< attr::UnknownAttribute >( 2, "ua2v" ) );

    size_t serializedSize;
    QByteArray serializedMessage;
    serializedMessage.reserve( 100 );

    MessageSerializer serializer;
    serializer.setMessage( std::move( response ) );
    ASSERT_EQ( serializer.serialize( &serializedMessage, &serializedSize ),
               nx_api::SerializerState::done );

    static const QByteArray MESSAGE = QByteArray(
            "0013" "0014" "2112A442"    // indication 3, lenght=12, magic cookie
            "EFCDAB896745230167452301"  // transaction id (rev)
            "0009" "0010" "00000401");  // error code 0x3 0x77
    ASSERT_EQ( serializedMessage.size(), serializedSize );
    ASSERT_EQ( MESSAGE, serializedMessage.toHex().toUpper() );
    */
}

TEST( StunMessageSerialization, Authentification )
{
    // TODO: implement
}

} // namespace nx_stun
