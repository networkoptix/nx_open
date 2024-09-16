// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/network/cloud/cloud_connect_type.h>
#include <nx/network/http/http_status.h>
#include <nx/utils/std/cpp20.h>
namespace nx::network::cloud {

/**
 * The tunnel connection statistics for one specific endpoint reported by the mediator in its'
 * ConnectResponse, i.e:
 *  - one traffic relay.
 *  - one udp hole punching endpoint of the server.
 *  - one tcp endpoint of the server.
 * Each instance of AbstractTunnelConnector will report these statistics.
*/
struct TunnelConnectStatistics
{
    // The type of connection attempted: proxy, udp hole punching, etc.
    ConnectType connectType = ConnectType::unknown;
    // The amount of time it took to get a response back from the endpoint.
    std::chrono::milliseconds responseTime;
    // The specific endpoint that was being connected to. server tcp or udp endpoint, or
    // relay hostname
    std::string remoteAddress;
    // If the connection is attempted via an HTTP API (i.e. traffic_relay), this is the status code
    // in the response. If a response was never received, then TunnelConnectResult::sysErrorCode
    // should be consulted instead.
    std::optional<nx::network::http::StatusCode::Value> httpStatusCode;

    auto operator<=>(const TunnelConnectStatistics&) const = default;
};

} // namespace nx::network::cloud
