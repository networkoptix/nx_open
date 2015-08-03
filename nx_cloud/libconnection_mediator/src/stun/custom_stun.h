/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <utils/network/socket_common.h>
#include <utils/network/stun/stun_message.h>
#include <utils/common/uuid.h>

namespace nx {
namespace hpm { // hpm stands for "Hole Punching Mediator"

namespace methods
{
    enum Value
    {
        /** Pings mediaserver endpoints and reports verified onese
         *  Request: SystemId, ServerId, PublicEndpointList, Authorization
         *  Response: PublicEndpointList(vrified) */
        PING = stun::MethodType::USER_METHOD,

        /** Registers mediaserver for external connections.
         *  Request: SystemId, ServerId, PublicEndpointList, Authorization */
        BIND,

        // TODO: specify bahavior
        LISTEN,
        CONNECT
    };
};

namespace error
{
    enum Value
    {
        NOT_FOUND = 404,
    };
}

//TODO custom stun requests parameters
namespace attrs
{
    enum Value
    {
        SYSTEM_ID = stun::attrs::USER_DEFINED,
        SERVER_ID,
        CLIENT_ID,

        PUBLIC_ENDPOINT_LIST = stun::attrs::USER_DEFINED + 0x200,
        AUTHORIZATION,

    };

    struct Id : stun::attrs::Unknown
    {
        Id( int type, QnUuid value ) : stun::attrs::Unknown( type, value.toRfc4122() ) {}
        QnUuid get() const { return QnUuid::fromRfc4122( value ); }
    };

    struct SystemId : Id
    {
        static const int TYPE = SYSTEM_ID;
        SystemId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct ServerId : Id
    {
        static const int TYPE = SERVER_ID;
        ServerId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct ClientId : Id
    {
        static const int TYPE = CLIENT_ID;
        ClientId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct PublicEndpointList : stun::attrs::Unknown
    {
        static const int TYPE = PUBLIC_ENDPOINT_LIST;
        PublicEndpointList( const std::list< SocketAddress >& endpoints );
        std::list< SocketAddress > get() const;
    };

    struct Authorization : stun::attrs::Unknown
    {
        static const int TYPE = AUTHORIZATION;
        Authorization( nx::String value ) : stun::attrs::Unknown( TYPE, value ) {}

        // TODO: introduce some way of parsing value
    };
}

} // namespace hpm
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
