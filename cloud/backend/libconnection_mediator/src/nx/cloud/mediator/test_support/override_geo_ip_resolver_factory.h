#pragma once

#include <memory>

#include <nx/cloud/mediator/geo_ip/resolver_factory.h>
#include <nx/geo_ip/test_support/memory_resolver.h>

namespace nx::hpm::test {

class OverrideGeoIpResolverFactory
{
public:
    OverrideGeoIpResolverFactory()
    {
        m_factoryFuncBak = geo_ip::ResolverFactory::instance().setCustomFunc(
            [this](const conf::Settings& /*settings*/)
            {
                return std::make_unique<nx::geo_ip::test::MemoryResolver>();
            });
    }

    ~OverrideGeoIpResolverFactory()
    {
        if (m_factoryFuncBak)
            geo_ip::ResolverFactory::instance().setCustomFunc(std::move(m_factoryFuncBak));
    }
private:
    nx::utils::MoveOnlyFunc<geo_ip::ResolverFactoryFunction> m_factoryFuncBak;
};

} //namespace nx::hpm::test
