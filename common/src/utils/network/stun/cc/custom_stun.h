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
        hostName = stun::attrs::userDefined,
        userName,

        publicEndpointList = stun::attrs::userDefined + 0x200,
        authorization,
    };

    struct HostName : stun::attrs::Unknown
    {
        static const int TYPE = hostName;
        HostName( nx::String value ) : stun::attrs::Unknown( TYPE, value ) {}
    };

    struct UserName : stun::attrs::Unknown
    {
        static const int TYPE = userName;
        UserName( nx::String value ) : stun::attrs::Unknown( TYPE, value ) {}
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

} // namespace cc
} // namespace stun
} // namespace nx

#endif  //NX_CUSTOM_STUN_H
