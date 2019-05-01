#pragma once

#include <nx/utils/basic_factory.h>

#include "abstract_geo_ip_resolver.h"

namespace nx::hpm {

namespace conf { class Settings; }

namespace geo_ip {

using GeoIpResolverFactoryFunction =
std::unique_ptr<AbstractGeoIpResolver>(const conf::Settings&);

class GeoIpResolverFactory:
    public nx::utils::BasicFactory<GeoIpResolverFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<GeoIpResolverFactoryFunction>;
public:
    GeoIpResolverFactory();

    static GeoIpResolverFactory& instance();

private:
    std::unique_ptr<AbstractGeoIpResolver>
        defaultFactoryFunction(const conf::Settings& settings);
};

} // namespace geo_ip
} // namespace nx::hpm: