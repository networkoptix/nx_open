// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "udp_socket_pair.h"

#include <nx/network/socket_factory.h>

namespace nx::streaming::rtsp {

QString UdpSocketPair::getPortsString()
{
    if (!mediaSocket || !rtcpSocket)
        return QString();

    return QString::number(mediaSocket->getLocalAddress().port) + "-" +
        QString::number(rtcpSocket->getLocalAddress().port);
}

bool UdpSocketPair::bind()
{
    // Try to find a pair of ports even for RTP, odd for RTCP.
    auto bindSocket = [](int port)
    {
        auto socket = nx::network::SocketFactory::createDatagramSocket();
        if (socket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, port)))
            return socket;
        return std::unique_ptr<nx::network::AbstractDatagramSocket>();
    };

    mediaSocket = bindSocket(0);
    if (!mediaSocket)
    {
        NX_WARNING(this, "Failed to bind UDP socket to any port");
        return false;
    }

    int port = mediaSocket->getLocalAddress().port;
    if (port & 1) // Odd port
        mediaSocket = bindSocket(++port);

    if (mediaSocket)
        rtcpSocket = bindSocket(port + 1);

    while (port < 65534 && (!mediaSocket || !rtcpSocket))
    {
        port += 2;
        mediaSocket = bindSocket(port);
        if (mediaSocket)
            rtcpSocket = bindSocket(port + 1);
    }

    if (!rtcpSocket)
        mediaSocket.reset();

    return mediaSocket && rtcpSocket;
}

}

