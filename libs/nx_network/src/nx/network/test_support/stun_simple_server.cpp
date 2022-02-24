// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_simple_server.h"

#include <nx/network/stun/stun_types.h>

namespace nx::network::stun::test {

SimpleServer::SimpleServer():
    base_type(&m_dispatcher)
{
}

SimpleServer::~SimpleServer()
{
    pleaseStopSync();
}

nx::utils::Url SimpleServer::url() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::stun::kUrlSchemeName).setEndpoint(address());
}

nx::network::stun::MessageDispatcher& SimpleServer::dispatcher()
{
    return m_dispatcher;
}

void SimpleServer::sendIndicationThroughEveryConnection(nx::network::stun::Message message)
{
    forEachConnection(
        nx::network::server::MessageSender<nx::network::stun::ServerConnection>(std::move(message)));
}

} // namespace nx::network::stun::test
