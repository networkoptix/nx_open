/**********************************************************
* 20 dec 2013
* a.kolesnikov
***********************************************************/

#include "message.h"
#include "message_serializer.h"

#include <openssl/hmac.h>

static const size_t DEFAULT_BUFFER_SIZE = 4 * 1024;

namespace nx {
namespace stun {

Header::Header()
    : messageClass( MessageClass::request )
    , method( 0 )
    , transactionId( TRANSACTION_ID_SIZE, 0 )
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

static Buffer randomBuffer( int size )
{
    Buffer id( size, 0 );
    std::generate( id.begin(), id.end(), qrand );
    return id;
}

Buffer Header::makeTransactionId()
{
    return randomBuffer( TRANSACTION_ID_SIZE );
}

namespace attrs
{
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
        : buffer( std::move( buffer_ ) )
    {
    }

    const Buffer& BufferedValue::getBuffer() const
    {
        return buffer;
    }

    String BufferedValue::getString() const
    {
        return bufferToString( buffer );
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
    serializer.setMessage( message );
    Q_ASSERT( serializer.serialize( &buffer, &bytes )
              == nx_api::SerializerState::done );

    return hmacSha1( key, buffer );
}

void Message::insertIntegrity( const String& userName, const String& key )
{
    newAttribute< attrs::UserName >( userName );
    newAttribute< attrs::Nonce >( randomBuffer( 10 ).toHex() );
    newAttribute< attrs::MessageIntegrity >( nx::Buffer(
        attrs::MessageIntegrity::SIZE, 0 ) );

    const auto hmac = hmacSha1( key, this );
    Q_ASSERT( hmac.size() == attrs::MessageIntegrity::SIZE );
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

Message::Message( Header header_, AttributesMap attributes_ )
    : header( std::move(header_) )
    , attributes( std::move(attributes_) )
{
}

void Message::addAttribute( std::unique_ptr<attrs::Attribute>&& attribute )
{
    attributes[ attribute->getType() ] = std::move( attribute );
}

void Message::clear()
{
    attributes.clear();
}

} // namespase stun
} // namespase nx
