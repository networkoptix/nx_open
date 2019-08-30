#pragma once

#include <nx/geo_ip/abstract_resolver.h>

#include <nx/utils/basic_factory.h>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }

namespace controller {

using GeoIpResolverFactoryType =
    std::unique_ptr<nx::geo_ip::AbstractResolver>(const conf::Settings&);

class GeoIpResolverFactory:
    public nx::utils::BasicFactory<GeoIpResolverFactoryType>
{
    using base_type = nx::utils::BasicFactory<GeoIpResolverFactoryType>;

public:
    GeoIpResolverFactory();

    static GeoIpResolverFactory& instance();

private:
    std::unique_ptr<nx::geo_ip::AbstractResolver>
        defaultFactoryFunction(const conf::Settings& settings);
};

} // namespace controller
} // namespace nx::cloud::storage::service