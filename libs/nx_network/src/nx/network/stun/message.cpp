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
namespace network {
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
    return result;
}

static Buffer hmacSha1( const String& key, const Message* message )
{
    Buffer buffer;
    buffer.reserve( DEFAULT_BUFFER_SIZE );

    size_t bytes;
    MessageSerializer serializer;
    serializer.setMessage(message);
    if (serializer.serialize(&buffer, &bytes) != nx::network::server::SerializerState::done)
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
        if( const auto err = getAttribute< attrs::ErrorCode >() )
        {
            return QString( lm( "STUN error %1: %2" )
                .arg( err->getCode() ).arg( err->getString() ) );
        }
        else
        {
            return QString( lm( "STUN error without ErrorCode" ) );
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

} // namespace stun
} // namespace network
} // namespace nx
