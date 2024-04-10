// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/network/http/server/http_statistics.h>
#include <nx/reflect/instrument.h>

namespace nx::hpm::api {

template<typename T>
using Percentiles = std::map<int /*percentile (50, 95, 99)*/, T>;

using PercentilesMs = Percentiles<std::chrono::milliseconds>;


struct ConnectTypeStatistics
{
    int count = 0;
    PercentilesMs responseTimePercentiles;
};
NX_REFLECTION_INSTRUMENT(ConnectTypeStatistics, (count)(responseTimePercentiles))


struct HostStatistics
{
    std::map<std::string /*errorCode*/, int /*count*/> sysErrorCodes;
    //NOTE: using int here for the http status code so that json shows "502" instead of "Service Unavailable"
    std::optional<std::map<int/*http status code*/, int /*count*/>> httpStatusCodes;
};
NX_REFLECTION_INSTRUMENT(HostStatistics, (sysErrorCodes)(httpStatusCodes))


struct NatTraversalResultCodeStatistics
{
    int count = 0;
    std::map<std::string /*hostname*/, HostStatistics> hosts;
};
NX_REFLECTION_INSTRUMENT(NatTraversalResultCodeStatistics, (count)(hosts))


struct CloudConnectStatistics
{
    int serverCount = 0;
    int clientCount = 0;

    int totalConnectionsEstablishedPerMinute = 0;
    std::map<nx::network::cloud::ConnectType, ConnectTypeStatistics> establishedConnectionsPerType;

    int totalConnectionsFailedPerMinute = 0;
    std::map<api::NatTraversalResultCode, NatTraversalResultCodeStatistics> failedConnectionsPerResultCode;

    // Response times from the mediator for successful connections.
    PercentilesMs mediatorResponseTimePercentiles;
};

#define CloudConnectStatistics_Fields (serverCount)(clientCount) \
    (totalConnectionsEstablishedPerMinute)(totalConnectionsFailedPerMinute) \
    (establishedConnectionsPerType)(failedConnectionsPerResultCode) \
    (mediatorResponseTimePercentiles)

NX_REFLECTION_INSTRUMENT(CloudConnectStatistics, CloudConnectStatistics_Fields)

//-------------------------------------------------------------------------------------------------

struct Country
{
    int total = 0;
    std::map<std::string/*name*/, int/*total*/> city;
    std::map<std::string/*subdivision*/, int /*total*/> subdivision;
};

#define Country_mediator_Fields (total)(city)(subdivision)

NX_REFLECTION_INSTRUMENT(Country, Country_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Continent
{
    int total = 0;
    std::map<std::string/*name*/, Country> country;
};

#define Continent_mediator_Fields (total)(country)

NX_REFLECTION_INSTRUMENT(Continent, Continent_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Location
{
    std::map<std::string/*name*/, Continent> continent;
};

#define Location_mediator_Fields (continent)

NX_REFLECTION_INSTRUMENT(Location, Location_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct ListeningPeerStatistics
{
    Location location;

    int peerCount() const
    {
        return std::accumulate(
            location.continent.begin(), location.continent.end(),
            0,
            [](int count, const std::pair<std::string/*name*/, Continent>& val)
            {
                return count + val.second.total;
            });
    }
};

#define ListeningPeerStatistics_mediator_Fields (location)

NX_REFLECTION_INSTRUMENT(ListeningPeerStatistics, ListeningPeerStatistics_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Statistics
{
    network::http::server::HttpStatistics http;
    network::server::Statistics stun;
    CloudConnectStatistics cloudConnect;
};

#define Statistics_mediator_Fields (http)(stun)(cloudConnect)

NX_REFLECTION_INSTRUMENT(Statistics, Statistics_mediator_Fields)

} // namespace nx::hpm::api
