/**********************************************************
* 20 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message.h"

namespace nx {
namespace stun {

    Header::Header( MessageClass messageClass_ , int method_,
                    TransactionID transactionID_ )
        : messageClass( messageClass_ )
        , method( method_ )
        , transactionID( transactionID_ )
    {
    }

    namespace attr
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

        ErrorDescription::ErrorDescription( int code_, nx::String phrase )
            : code( code_ )
            , reasonPhrase( phrase )
        {
        }

        FingerPrint::FingerPrint( uint32_t crc32_ )
            : crc32( crc32_ )
        {
        }

        UnknownAttribute::UnknownAttribute( int userType_, nx::Buffer value_ )
            : userType( userType_ )
            , value( std::move( value_ ) )
        {
        }
    }

    Message::Message( Header header_ )
        : header( header_ )
    {
    }

    Message::Message( Message&& message )
        : header( std::move(message.header) )
        , attributes( std::move(message.attributes) )
    {
    }

    Message& Message::operator = ( Message&& message )
    {
        if( this == &message ) return *this;
        attributes = std::move(message.attributes);
        header = message.header;
        return *this;
    }

    void Message::addAttribute( std::unique_ptr<attr::Attribute>&& attribute )
    {
        attributes.insert( AttributesMap::value_type(
                attribute->type(), std::move( attribute ) ) );
    }

    void Message::clear()
    {
        attributes.clear();
    }

} // namespase stun
} // namespase nx
