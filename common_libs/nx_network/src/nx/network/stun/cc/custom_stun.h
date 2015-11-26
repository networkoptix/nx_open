/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CUSTOM_STUN_H
#define NX_CUSTOM_STUN_H

#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace stun {
namespace cc {

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

        hostName = stun::attrs::userDefined + 0x200,
        publicEndpointList,
        authorization,
    };

    struct NX_NETWORK_API StringAttribute : stun::attrs::Unknown
    {
        StringAttribute( int userType, const String& value = String() );
    };

    struct NX_NETWORK_API SystemId : StringAttribute
    {
        static const int TYPE = systemId;
        SystemId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ServerId : StringAttribute
    {
        static const int TYPE = serverId;
        ServerId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API ClientId : StringAttribute
    {
        static const int TYPE = clientId;
        ClientId( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API HostName : StringAttribute
    {
        static const int TYPE = hostName;
        HostName( const String& value ) : StringAttribute( TYPE, value ) {}
    };

    struct NX_NETWORK_API PublicEndpointList : StringAttribute
    {
        static const int TYPE = publicEndpointList;
        PublicEndpointList( const std::list< SocketAddress >& endpoints );
        std::list< SocketAddress > get() const;
    };
}

} // namespace cc
} // namespace stun
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
