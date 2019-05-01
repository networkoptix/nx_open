#pragma once

#include "maxmind_geo_ip_resolver.h"

#include <nx/maxmind/maxmind_db.h>

#include "nx/cloud/mediator/settings.h"

namespace nx::hpm::geo_ip {

namespace {

Continent toContinent(maxmind::Continent continent)
{
    switch (continent)
    {
        case maxmind::Continent::africa:
            return Continent::africa;
        case maxmind::Continent::antarctica:
            return Continent::antarctica;
        case maxmind::Continent::asia:
            return Continent::asia;
        case maxmind::Continent::australia:
            return Continent::australia;
        case maxmind::Continent::europe:
            return Continent::europe;
        case maxmind::Continent::northAmerica:
            return Continent::northAmerica;
        case maxmind::Continent::southAmerica:
        default:
            return Continent::southAmerica;
    }
}

ResultCode toResultCode(maxmind::ResultCode resultCode)
{
    switch (resultCode)
    {
        case maxmind::ResultCode::ok:
            return ResultCode::ok;
        case maxmind::ResultCode::ioError:
            return ResultCode::ioError;
        case maxmind::ResultCode::unknownError:
        default:
            return ResultCode::unknownError;
    }
}

} //namespace

MaxmindGeoIpResolver::MaxmindGeoIpResolver(const conf::Settings& settings):
    m_geoIp(settings.geoIp()),
    m_db(std::make_unique<nx::maxmind::MaxmindDb>())
{
    if (!m_db->open(m_geoIp.dbPath))
    {
        NX_VERBOSE(this, "Error opening maxmind db, ip resolution will not function...");
        m_db.reset();
    }
    auto[a, b] = m_db->lookup("41.77.112.248"); // africa
    auto[a1, b1] = m_db->lookup("202.144.196.2"); // antarctica
    auto[a2, b2] = m_db->lookup("1.10.0.1"); // asia
    auto[a3, b3] = m_db->lookup("17.86.36.1"); // australia
    auto[a4, b4] = m_db->lookup("2.16.126.1"); // europe
    auto[a5, b5] = m_db->lookup("8.8.8.8");
    auto[a6, b6] = m_db->lookup("2.16.188.10"); // south america
}

std::pair<ResultCode, Continent> MaxmindGeoIpResolver::resolve(const std::string& ipAddress)
{
    if (!m_db)
        return {ResultCode::ioError, {}};

    auto[resultCode, location] = m_db->lookup(ipAddress);

    return {toResultCode(resultCode), toContinent(location.continent)};
}

}
