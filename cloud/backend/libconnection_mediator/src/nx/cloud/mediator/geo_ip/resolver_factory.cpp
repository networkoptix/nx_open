#include "resolver_factory.h"

#include <nx/geo_ip/resolver.h>

#include "nx/cloud/mediator/settings.h"

namespace nx::hpm::geo_ip {

ResolverFactory::ResolverFactory():
    base_type(std::bind(&ResolverFactory::defaultFactoryFunction,
        this, std::placeholders::_1))
{
}

ResolverFactory& ResolverFactory::instance()
{
    static ResolverFactory instance;
    return instance;
}

std::unique_ptr<nx::geo_ip::AbstractResolver>
ResolverFactory::defaultFactoryFunction(const conf::Settings& settings)
{
    return std::make_unique<nx::geo_ip::Resolver>(settings.geoIp());
}

} // namespace nx::hpm::geo_ip