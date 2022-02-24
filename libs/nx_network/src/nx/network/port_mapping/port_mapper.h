// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef PORT_MAPPER_H
#define PORT_MAPPER_H

#include <nx/network/socket_factory.h>

namespace nx {
namespace network {

class PortMapping
{
    SocketAddress external;
    quint32 requestTime;
    quint32 lifeTime;

    PortMapping();
    quint32 timeLeft();
};

/*
class NatPnpPortMapper
{
public:
    NatPnpPortMapper(const HostAddress& server);

    virtual bool mapPort(PortMapping& portMapping);
};
*/

} // namespace network
} // namespace nx

#endif // PORT_MAPPER_H
