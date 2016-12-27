/**********************************************************
* 20 dec 2013
* a.kolesnikov
***********************************************************/

#include "message.h"

#include <mutex>
#include <random>

#include <openssl/hmac.h>

#include <nx/utils/random.h>

#include "message_parser.h"
#include "message_serializer.h"
#include "stun_message_parser_buffer.h"
#include "stun_message_serializer_buffer.h"

static const size_t DEFAULT_BUFFER_SIZE = 4 * 1024;

namespace nx {
namespace stun {

Header::Header()
    : messageClass(MessageClass::unknown)
    , method(MethodType::invalid)
    , transactionId(nullTransactionId)
{
}

Header::Header(const Header& right)
:
    messageClass(right.messageClass),
    method(right.method),
    transactionId(right.transactionId)
{
}

Header::Header(Header&& right)
:
    messageClass(std::move(right.messageClass)),
    method(std::move(right.method)),
    transactionId(std::move(right.transactionId))
{
}
         
Header::Header( MessageClass messageClass_ , int method_)
    : messageClass( messageClass_ )
    , method( method_ )
    , transactionId( makeTransactionId() )
{
}

Header::Header( MessageClass messageClass_ , int method_,
                Buffer transactionId_ )
    : messageClass( messageClass_ )
    , method( method_ )
    , transactionId( std::move( transactionId_ ) )
{
}

Header& Header::operator=(Header&& rhs)
{
    if (this != &rhs)
    {
        messageClass = std::move(rhs.messageClass);
        method = std::move(rhs.method);
        transactionId = std::move(rhs.transactionId);
    }
    return *this;
}

Buffer Header::makeTransactionId()
{
    return nx::utils::random::generate(TRANSACTION_ID_SIZE);
}

Buffer Header::nullTransactionId(TRANSACTION_ID_SIZE, 0);

namespace attrs {

//-------------------------------------------------------------------------------------------------
// class AttributeFactory

std::unique_ptr<SerializableAttribute> AttributeFactory::create(int attributeType)
{
    switch (attributeType)
    {
        case attrs::mappedAddress:
            return std::make_unique<MappedAddress>();

        case attrs::alternateServer:
            return std::make_unique<AlternateServer>();

        default:
            return nullptr;
    }
}

//-------------------------------------------------------------------------------------------------
// class MappedAddress

MappedAddress::MappedAddress()
{
}

MappedAddress::MappedAddress(SocketAddress endpoint):
    m_endpoint(std::move(endpoint))
{
    NX_ASSERT(m_endpoint.address.isIpAddress());
    NX_ASSERT(m_endpoint.address.ipV4() || m_endpoint.address.ipV6());
}

int MappedAddress::getType() const
{
    return TYPE;
}

nx_api::SerializerState::Type MappedAddress::serialize(
    MessageSerializerBuffer* buffer,
    std::size_t* bytesWritten) const
{
    const auto initialPosition = buffer->position();

    if (buffer->WriteByte(0) == nullptr)
        return nx_api::SerializerState::needMoreBufferSpace;

    const uint8_t addressFamily = m_endpoint.address.ipV4() ? kAddressTypeIpV4 : kAddressTypeIpV6;
    if (buffer->WriteByte(addressFamily) == nullptr)
        return nx_api::SerializerState::needMoreBufferSpace;

    if (buffer->WriteUint16(m_endpoint.port) == nullptr)
        return nx_api::SerializerState::needMoreBufferSpace;

    if (const auto ipv4Address = m_endpoint.address.ipV4())
    {
        if (buffer->WriteBytes(reinterpret_cast<const char*>(&ipv4Address->s_addr), 4) == nullptr)
            return nx_api::SerializerState::needMoreBufferSpace;
    }
    else if(const auto ipv6Address = m_endpoint.address.ipV6())
    {
        if (buffer->WriteBytes(reinterpret_cast<const char*>(&ipv6Address->s6_addr), 16) == nullptr)
            return nx_api::SerializerState::needMoreBufferSpace;
    }

    *bytesWritten = buffer->position() - initialPosition;

    return nx_api::SerializerState::done;
}

bool MappedAddress::deserialize(MessageParserBuffer* buffer)
{
    bool ok = false;

    const uint8_t zeroByte = buffer->NextByte(&ok);
    if (!ok)
        return false;
    if (zeroByte != 0)
        return false;

    const uint8_t addressFamily = buffer->NextByte(&ok);
    if (!ok)
        return false;
    if (addressFamily != kAddressTypeIpV4 && addressFamily != kAddressTypeIpV6)
        return false;

    m_endpoint.port = buffer->NextUint16(&ok);
    if (!ok)
        return false;

    switch (addressFamily)
    {
        case kAddressTypeIpV4:
        {
            in_addr addr;
            memset(&addr, 0, sizeof(addr));
            buffer->readNextBytesToBuffer(reinterpret_cast<char*>(&addr.s_addr), 4, &ok);
            if (!ok)
                return false;
            m_endpoint.address = HostAddress(addr);
            break;
        }

        case kAddressTypeIpV6:
        {
            in6_addr addr;
            memset(&addr, 0, sizeof(addr));
            buffer->readNextBytesToBuffer(reinterpret_cast<char*>(&addr.s6_addr), 16, &ok);
            if (!ok)
                return false;
            m_endpoint.address = HostAddress(addr);
            break;
        }

        default:
            return false;
    }

    return true;
}

const SocketAddress& MappedAddress::endpoint() const
{
    return m_endpoint;
}

//-------------------------------------------------------------------------------------------------
// class AlternateServer

AlternateServer::AlternateServer()
{
}

AlternateServer::AlternateServer(SocketAddress endpoint):
    MappedAddress(std::move(endpoint))
{
}

int AlternateServer::getType() const
{
    return TYPE;
}

//-------------------------------------------------------------------------------------------------
// class XorMappedAddress

XorMappedAddress::XorMappedAddress()
    : family( 0 ) // invalid
    , port( 0 )
{
}

XorMappedAddress::XorMappedAddress( int port_, uint32_t ipv4_ )
    : family( IPV4 )
    , port( port_ )
{
    address.ipv4 = ipv4_;
}

XorMappedAddress::XorMappedAddress( int port_, Ipv6 ipv6_ )
    : family( IPV6 )
    , port( port_ )
{
    address.ipv6 = ipv6_;
}

BufferedValue::BufferedValue( nx::Buffer buffer_ )
    : m_buffer( std::move( buffer_ ) )
{
}

const Buffer& BufferedValue::getBuffer() const
{
    return m_buffer;
}

void BufferedValue::setBuffer(Buffer buf)
{
    m_buffer = std::move(buf);
}

String BufferedValue::getString() const
{
    return bufferToString(m_buffer);
}

UserName::UserName( const String& value )
    : BufferedValue( stringToBuffer( value ) )
{
}

ErrorDescription::ErrorDescription( int code_, const nx::String& phrase )
    : BufferedValue( bufferToString( phrase ) )
    , code( code_ )
{
}

FingerPrint::FingerPrint( uint32_t crc32_ )
    : crc32( crc32_ )
{
}


MessageIntegrity::MessageIntegrity( nx::Buffer hmac )
    : BufferedValue( std::move( hmac ) )
{
}

Nonce::Nonce( Buffer nonce )
    : BufferedValue( std::move( nonce ) )
{
}

Unknown::Unknown( int userType_, nx::Buffer value )
    : BufferedValue( std::move( value ) )
    , userType( userType_ )
{
}


////////////////////////////////////////////////////////////
//// IntAttribute
////////////////////////////////////////////////////////////

IntAttribute::IntAttribute(int userType, int value)
:
    Unknown(userType)
{
    const int valueInNetworkByteOrder = htonl(value);
    setBuffer(
        Buffer(
            reinterpret_cast<const char*>(&valueInNetworkByteOrder),
            sizeof(valueInNetworkByteOrder)));
}

int IntAttribute::value() const
{
    const auto& buf = getBuffer();
    int valueInNetworkByteOrder = 0;
    if (buf.size() != sizeof(valueInNetworkByteOrder))
        return 0;
    memcpy(
        &valueInNetworkByteOrder,
        buf.constData(),
        sizeof(valueInNetworkByteOrder));
    return ntohl(valueInNetworkByteOrder);
}
}

// provided by http://wiki.qt.io/HMAC-SHA1
static Buffer hmacSha1( const String& key, const String& baseString )
{
    unsigned int len;
    Buffer result( 20, 0 );

    HMAC_CTX ctx;
    HMAC_CTX_init( &ctx);
    HMAC_Init_ex( &ctx, key.data(), key.size(), EVP_sha1(), NULL );
    HMAC_Update( &ctx, reinterpret_cast<const unsigned char*>(
                     baseString.data() ), baseString.size() );
    HMAC_Final( &ctx, reinterpret_cast<unsigned char*>(
                    result.data() ), &len );
    HMAC_CTX_cleanup( &ctx );

    result.resize( len );
    return std::move( result );
}

static Buffer hmacSha1( const String& key, const Message* message )
{
    Buffer buffer;
    buffer.reserve( DEFAULT_BUFFER_SIZE );

    size_t bytes;
    MessageSerializer serializer;
    serializer.setMessage(message);
    if (serializer.serialize(&buffer, &bytes) != nx_api::SerializerState::done)
    {
        NX_ASSERT(false);
    }

    return hmacSha1( key, buffer );
}

void Message::insertIntegrity( const String& userName, const String& key )
{
    newAttribute< attrs::UserName >( userName );
    newAttribute< attrs::Nonce >( nx::utils::random::generate( 10 ).toHex() );
    newAttribute< attrs::MessageIntegrity >( nx::Buffer(
        attrs::MessageIntegrity::SIZE, 0 ) );

    const auto hmac = hmacSha1( key, this );
    NX_ASSERT( hmac.size() == attrs::MessageIntegrity::SIZE );
    newAttribute< attrs::MessageIntegrity >( hmac );
}

bool Message::verifyIntegrity( const String& userName, const String& key )
{
    const auto userNameAttr = getAttribute< attrs::UserName >();
    if( !userNameAttr || userNameAttr->getString() != userName )
        return false;

    const auto miAttr = getAttribute< attrs::MessageIntegrity >();
    if( !miAttr )
        return false;

    Buffer messageHmac = miAttr->getBuffer();
    newAttribute< attrs::MessageIntegrity >();

    Buffer realHmac = hmacSha1( key, this );
    return messageHmac == realHmac;
}

boost::optional< QString > Message::hasError( SystemError::ErrorCode code ) const
{
    if( code != SystemError::noError )
    {
        return QString( lm( "System error %1: %2" )
            .arg( code ).arg( SystemError::toString( code ) ) );
    }

    if( header.messageClass != MessageClass::successResponse )
    {
        if( const auto err = getAttribute< attrs::ErrorDescription >() )
        {
            return QString( lm( "STUN error %1: %2" )
                .arg( err->getCode() ).arg( err->getString() ) );
        }
        else
        {
            return QString( lm( "STUN error without ErrorDescription" ) );
        }
    }

    return boost::none;
}

Message::Message( Header header_, AttributesMap attributes_ )
    : header( std::move(header_) )
    , attributes( std::move(attributes_) )
{
}

void Message::addAttribute( AttributePtr&& attribute )
{
    attributes[ attribute->getType() ] = std::move( attribute );
}

void Message::clear()
{
    attributes.clear();
}

} // namespase stun
} // namespase nx
