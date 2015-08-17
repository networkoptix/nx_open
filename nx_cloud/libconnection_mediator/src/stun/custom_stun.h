/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <utils/network/socket_common.h>
#include <utils/network/stun/message.h>
#include <utils/common/uuid.h>

namespace nx {
namespace hpm { // hpm stands for "Hole Punching Mediator"

namespace methods
{
    enum Value
    {
        /** Pings mediaserver endpoints and reports verified onese
         *  Request: \class attrs::SystemId, \class attrs::ServerId,
         *           \class attrs::Authorization, \class attrs::PublicEndpointList
         *  Response: \class attrs::PublicEndpointList (vrified) */
        ping = stun::MethodType::userMethod,

        /** Registers mediaserver for external connections.
         *  Request: \class attrs::SystemId, \class attrs::ServerId,
         *           \class attrs::Authorization, \class attrs::PublicEndpointList */
        bind,

        // TODO: specify bahavior
        listen,
        connect
    };
};

namespace error
{
    enum Value
    {
        notFound = 404,
    };
}

//TODO custom stun requests parameters
namespace attrs
{
    enum Value
    {
        systemId = stun::attrs::userDefined,
        serverId,
        clientId,

        publicEndpointList = stun::attrs::userDefined + 0x200,
        authorization,

    };

    struct Id : stun::attrs::Unknown
    {
        Id( int type, QnUuid value ) : stun::attrs::Unknown( type, value.toRfc4122() ) {}
        QnUuid get() const { return QnUuid::fromRfc4122( value ); }
    };

    struct SystemId : Id
    {
        static const int TYPE = systemId;
        SystemId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct ServerId : Id
    {
        static const int TYPE = serverId;
        ServerId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct ClientId : Id
    {
        static const int TYPE = clientId;
        ClientId( QnUuid value ) : Id( TYPE, std::move(value) ) {}
    };

    struct PublicEndpointList : stun::attrs::Unknown
    {
        static const int TYPE = publicEndpointList;
        PublicEndpointList( const std::list< SocketAddress >& endpoints );
        std::list< SocketAddress > get() const;
    };

    struct Authorization : stun::attrs::Unknown
    {
        static const int TYPE = authorization;
        Authorization( nx::String value ) : stun::attrs::Unknown( TYPE, value ) {}

        // TODO: introduce some way of parsing value
    };
}

} // namespace hpm
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
