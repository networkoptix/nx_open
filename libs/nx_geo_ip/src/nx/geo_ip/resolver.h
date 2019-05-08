#pragma once

#include <string>
#include <memory>

#include "abstract_resolver.h"

namespace nx::geo_ip {

class Settings;

class NX_GEO_IP_API Resolver: public AbstractResolver
{
public:
    Resolver(const Settings& settings);
    ~Resolver();

    ResultCode initialize();

    virtual Result resolve(const nx::network::SocketAddress& endpoint) override;

private:
    ResultCode initializeInternal();

private:
    class ResolverImpl;

    const Settings& m_settings;
    std::unique_ptr<ResolverImpl> m_impl;
    ResultCode m_initCode = ResultCode::unknownError;
};

} // namespace nx::geo_ip