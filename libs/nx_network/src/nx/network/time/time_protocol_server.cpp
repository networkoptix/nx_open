// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_protocol_server.h"

namespace nx {
namespace network {

std::shared_ptr<TimeProtocolServer::ConnectionType> TimeProtocolServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> socket)
{
    return std::make_shared<TimeProtocolConnection>(std::move(socket));
}

} // namespace network
} // namespace nx
