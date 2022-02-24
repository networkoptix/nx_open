// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

#include <nx/network/abstract_socket.h>

namespace nx::streaming::rtsp {

struct NX_VMS_COMMON_API UdpSocketPair
{
    // Creates socket and try to find a pair of sequential ports even for RTP, odd for RTCP.
    bool bind();

    QString getPortsString();

    std::unique_ptr<nx::network::AbstractDatagramSocket> mediaSocket;
    std::unique_ptr<nx::network::AbstractDatagramSocket> rtcpSocket;
};

}
