#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/geo_ip/types.h>

namespace nx::geo_ip { class AbstractResolver; }

namespace nx::hpm{

class ListeningPeerPool;

namespace stats::geo_ip {

struct Country
{
    int total = 0;
    std::map<std::string/*name*/, int/*total*/> city;
    std::map<std::string/*subdivision*/, int /*total*/> subdivision;
};

struct Continent
{
    int total = 0;
    std::map<std::string/*name*/, Country> country;
};

struct Location
{
    std::map<std::string/*name*/, Continent> continent;
};

struct ListeningPeerStatistics
{
    Location location;
};

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ListeningPeerStatistics),
    (json))

//-------------------------------------------------------------------------------------------------
// StatisticsProvider

class StatisticsProvider
{
public:
    StatisticsProvider(
        nx::geo_ip::AbstractResolver* geoIpResolver,
        ListeningPeerPool* listeningPeerPool);

    ListeningPeerStatistics listeningPeers() const;

private:
    std::optional<nx::geo_ip::Location> resolve(const std::string& ipAddress) const;

private:
    nx::geo_ip::AbstractResolver* m_geoIpResolver = nullptr;
    ListeningPeerPool* m_listeningPeerPool = nullptr;
};

} // namespace stats::geo_ip
} // namespace nx::hpm


