// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_speed.h"

#include <nx/reflect/json.h>

namespace nx::hpm::api {

bool ConnectionSpeed::operator==(const ConnectionSpeed& other) const
{
    return bandwidth == other.bandwidth && pingTime == other.pingTime;
}

std::string ConnectionSpeed::toString() const
{
    return nx::reflect::json::serialize(*this);
}

bool PeerConnectionSpeed::operator==(const PeerConnectionSpeed& other) const
{
    return
        systemId == other.systemId
        && serverId == other.serverId
        && connectionSpeed == other.connectionSpeed;
}

std::string PeerConnectionSpeed::toString() const
{
    return nx::reflect::json::serialize(*this);
}

} // namespace nx::hpm::api
