#include "geo_ip_resolver.h"

#include <nx/geo_ip/resolver.h>

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::controller {

GeoIpResolverFactory::GeoIpResolverFactory():
    base_type(
        std::bind(&GeoIpResolverFactory::defaultFactoryFunction, this, std::placeholders::_1))
{
}

GeoIpResolverFactory& GeoIpResolverFactory::instance()
{
    static GeoIpResolverFactory factory;
    return factory;
}

std::unique_ptr<geo_ip::AbstractResolver> GeoIpResolverFactory::defaultFactoryFunction(
    const conf::Settings& settings)
{
    return std::make_unique<geo_ip::Resolver>(settings.geoIp().dbPath);
}

} // namesapce nx::cloud::storage::service::controller
