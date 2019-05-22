#pragma once

#include <string>
#include <memory>

#include "abstract_resolver.h"

namespace nx::geo_ip {

class NX_GEO_IP_API Resolver: public AbstractResolver
{
public:
    Resolver(const std::string& mmdbPath);
    ~Resolver();

    virtual Location resolve(const std::string& ipAddress) override;

private:
    class ResolverImpl;

    std::unique_ptr<ResolverImpl> m_impl;
};

} // namespace nx::geo_ip