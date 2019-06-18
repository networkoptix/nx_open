#include "geo_ip_statistics.h"

#include <nx/fusion/model_functions.h>
#include <nx/geo_ip/abstract_resolver.h>

#include "nx/cloud/mediator/listening_peer_pool.h"
#include "nx/cloud/mediator/peer_registrator.h"

namespace nx::hpm::stats::geo_ip {

namespace{

static const char kTotal[] = "total";
static const char kCity[] = "city";
static const char kSubdivision[] = "subdivision";
static const char kCountry[] = "country";
static const char kContinent[] = "continent";
static const char kLocation[] = "location";

QJsonObject toJsonObject(const std::map<std::string, int>& map)
{
    QJsonObject o;
    for (const auto& element : map)
        o.insert(element.first.c_str(), element.second);
    return o;
}

QJsonObject toJsonObject(const Country& country)
{
    return QJsonObject ({
        { kTotal, country.total },
        { kCity, toJsonObject(country.city) },
        { kSubdivision, toJsonObject(country.subdivision) } });
}


QJsonObject toJsonObject(const Continent& continent)
{
    QJsonObject o;
    o.insert(kTotal, continent.total);
    for (const auto& element : continent.country)
    {
        o.insert(
            element.first.c_str(),
            toJsonObject(element.second));
    }
    return QJsonObject({ {kCountry, std::move(o)} });
}

QJsonObject toJsonObject(const Location& location)
{
    QJsonObject o;
    for (const auto& element : location.continent)
        o.insert(element.first.c_str(), toJsonObject(element.second));
    return QJsonObject({ {kContinent, std::move(o)} });
}

QJsonObject toJsonObject(const ListeningPeerStatistics& listeningPeer)
{
    return QJsonObject({ {kLocation, toJsonObject(listeningPeer.location)} });
}

} // namespace

/**
 * Implementation of QnFusion's serialize().
 * Function header is created by QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES in header file.
 */
void serialize(
    QnJsonContext* /*context*/,
    ListeningPeerStatistics const& listeningPeerStats,
    QJsonValue* outValue)
{
    *outValue = toJsonObject(listeningPeerStats);
}

 /**
  * Required by use of QnFusion serialization macros but not used.
  */
bool deserialize(
    QnJsonContext* /*context*/,
    QJsonValue const& /*jsonValue*/,
    ListeningPeerStatistics* /*outListeningPeerStatistics*/)
{
    // Intentionally doing nothing.
    return true;
}

StatisticsProvider::StatisticsProvider(
    nx::geo_ip::AbstractResolver* geoIpResolver,
    ListeningPeerPool* listeningPeerPool)
    :
    m_geoIpResolver(geoIpResolver),
    m_listeningPeerPool(listeningPeerPool)
{
}

ListeningPeerStatistics StatisticsProvider::listeningPeers() const
{
    ListeningPeerStatistics listeningPeerStats;
    Location& locationStats = listeningPeerStats.location;

    for (const auto& connection : m_listeningPeerPool->getAllConnections())
    {
        if (const auto strongConnection = connection.lock())
        {
            auto location =
                resolve(strongConnection->getSourceAddress().address.toStdString());

            if (!location)
                continue;

            auto continentStr = nx::geo_ip::toString(location->continent);

            auto& continentStats = locationStats.continent[continentStr];
            ++continentStats.total;

            if (location->country.name.empty())
                continue;

            auto& countryStats = continentStats.country[location->country.name];
            ++countryStats.total;

            if (!location->country.subdivision.empty())
                ++countryStats.subdivision[location->country.subdivision];

            if (location->city.empty())
                continue;

            ++countryStats.city[location->city];
        }
    }

    return listeningPeerStats;
}

std::optional<nx::geo_ip::Location> StatisticsProvider::resolve(const std::string& ipAddress) const
{
    try
    {
        return m_geoIpResolver->resolve(ipAddress);
    }
    catch (const std::exception& e)
    {
        NX_VERBOSE(this, "Error resolving ipAddress: %1 to a location: %2", ipAddress, e.what());
        return std::nullopt;
    }
}

} // namespace nx::hpm::stats::geo_ip