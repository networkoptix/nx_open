#pragma once

#include "abstract_geo_ip_resolver.h"

namespace nx::maxmind { class MaxmindDb; }

namespace nx::hpm{

namespace conf{ class Settings; struct GeoIp; }

namespace geo_ip {

class MaxmindGeoIpResolver: public AbstractGeoIpResolver
{
public:
    MaxmindGeoIpResolver(const conf::Settings&);

    std::pair<ResultCode, Continent> resolve(const std::string& ipAddress) override;

private:
    const conf::GeoIp& m_geoIp;
    std::unique_ptr<nx::maxmind::MaxmindDb> m_db;
};

} // namspace geo_ip
} // namespace nx::hpm