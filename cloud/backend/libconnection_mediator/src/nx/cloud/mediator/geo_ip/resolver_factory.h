#pragma once

#include <nx/utils/basic_factory.h>

#include <nx/geo_ip/abstract_resolver.h>

namespace nx::hpm {

namespace conf { class Settings; }

namespace geo_ip {

using ResolverFactoryFunction =
    std::unique_ptr<nx::geo_ip::AbstractResolver>(const conf::Settings&);

class ResolverFactory:
    public nx::utils::BasicFactory<ResolverFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ResolverFactoryFunction>;
public:
    ResolverFactory();

    static ResolverFactory& instance();

private:
    std::unique_ptr<nx::geo_ip::AbstractResolver>
        defaultFactoryFunction(const conf::Settings& settings);
};

} // namespace geo_ip
} // namespace nx::hpm