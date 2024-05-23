// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/reflect/instrument.h>

namespace nx::hpm::api {

struct NX_NETWORK_API ConnectionSpeed
{
    /**%apidoc Round-trip time from the peer to the Cloud. */
    std::chrono::milliseconds pingTime = std::chrono::milliseconds::zero();

    /**%apidoc Bandwidth in Kbps. */
    int bandwidth = 0;

    bool operator==(const ConnectionSpeed& other) const;

    std::string toString() const;
};

#define ConnectionSpeed_Fields (pingTime)(bandwidth)

NX_REFLECTION_INSTRUMENT(ConnectionSpeed, ConnectionSpeed_Fields)

struct NX_NETWORK_API PeerConnectionSpeed
{
    /**%apidoc Id of the server within the system. */
    std::string serverId;

    /**%apidoc Id of the system. This is the same id clients use to connect to the system. */
    std::string systemId;

    /** The measured connection bandwidth. */
	ConnectionSpeed connectionSpeed;

    bool operator==(const PeerConnectionSpeed& other) const;

    std::string toString() const;
};

#define PeerConnectionSpeed_Fields (serverId)(systemId)(connectionSpeed)

NX_REFLECTION_INSTRUMENT(PeerConnectionSpeed, PeerConnectionSpeed_Fields)

} // namespace nx::hpm::api
