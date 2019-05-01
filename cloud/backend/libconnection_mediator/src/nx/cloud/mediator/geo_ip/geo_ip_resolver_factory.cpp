#include "geo_ip_resolver_factory.h"

#include "nx/cloud/mediator/settings.h"

#include "maxmind_geo_ip_resolver.h"

namespace nx::hpm::geo_ip {

GeoIpResolverFactory::GeoIpResolverFactory():
    base_type(std::bind(&GeoIpResolverFactory::defaultFactoryFunction,
        this, std::placeholders::_1))
{
}

GeoIpResolverFactory& GeoIpResolverFactory::instance()
{
    static GeoIpResolverFactory instance;
    return instance;
}

std::unique_ptr<AbstractGeoIpResolver>
GeoIpResolverFactory::defaultFactoryFunction(const conf::Settings& settings)
{
    return std::make_unique<MaxmindGeoIpResolver>(settings);
}

} // namespace nx::hpm::geo_ip