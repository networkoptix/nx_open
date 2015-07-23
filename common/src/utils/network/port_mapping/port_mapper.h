#ifndef PORT_MAPPER_H
#define PORT_MAPPER_H

#include "utils/network/socket_factory.h"

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

#endif // PORT_MAPPER_H
