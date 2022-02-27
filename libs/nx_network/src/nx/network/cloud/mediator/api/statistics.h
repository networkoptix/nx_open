// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include <nx/network/connection_server/server_statistics.h>
#include <nx/network/cloud/data/connection_result_data.h>
#include <nx/network/http/server/http_statistics.h>

namespace nx::hpm::api {

struct CloudConnectStatistics
{
    int serverCount = 0;
    int clientCount = 0;
    int totalConnectionsEstablishedPerMinute = 0;
    std::map<nx::network::cloud::ConnectType, int> establishedConnectionsPerType;
    int totalConnectionsFailedPerMinute = 0;
    std::map<api::NatTraversalResultCode, int> failedConnectionsPerResultCode;
};

#define CloudConnectStatistics_Fields (serverCount)(clientCount) \
    (totalConnectionsEstablishedPerMinute)(totalConnectionsFailedPerMinute) \
    (establishedConnectionsPerType)(failedConnectionsPerResultCode)

QN_FUSION_DECLARE_FUNCTIONS(CloudConnectStatistics, (json))

NX_REFLECTION_INSTRUMENT(CloudConnectStatistics, CloudConnectStatistics_Fields)

//-------------------------------------------------------------------------------------------------

struct Country
{
    int total = 0;
    std::map<std::string/*name*/, int/*total*/> city;
    std::map<std::string/*subdivision*/, int /*total*/> subdivision;
};

#define Country_mediator_Fields (total)(city)(subdivision)

QN_FUSION_DECLARE_FUNCTIONS(Country, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(Country, Country_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Continent
{
    int total = 0;
    std::map<std::string/*name*/, Country> country;
};

#define Continent_mediator_Fields (total)(country)

QN_FUSION_DECLARE_FUNCTIONS(Continent, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(Continent, Continent_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Location
{
    std::map<std::string/*name*/, Continent> continent;
};

#define Location_mediator_Fields (continent)

QN_FUSION_DECLARE_FUNCTIONS(Location, (json), NX_NETWORK_API)

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

QN_FUSION_DECLARE_FUNCTIONS(ListeningPeerStatistics, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(ListeningPeerStatistics, ListeningPeerStatistics_mediator_Fields)

//-------------------------------------------------------------------------------------------------

struct Statistics
{
    network::http::server::HttpStatistics http;
    network::server::Statistics stun;
    CloudConnectStatistics cloudConnect;
};

#define Statistics_mediator_Fields (http)(stun)(cloudConnect)

QN_FUSION_DECLARE_FUNCTIONS(Statistics, (json), NX_NETWORK_API)

NX_REFLECTION_INSTRUMENT(Statistics, Statistics_mediator_Fields)

} // namespace nx::hpm::api
