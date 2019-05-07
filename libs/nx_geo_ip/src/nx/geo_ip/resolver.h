#pragma once

#include <string>
#include <memory>

#include "abstract_resolver.h"

namespace nx::geo_ip {

namespace detail { class ResolverImpl; }
class Settings;

class NX_GEO_IP_API Resolver: public AbstractResolver
{
public:
    Resolver(const Settings& settings);
    virtual ~Resolver() override;

    ResultCode initialize();

    virtual Result resolve(const nx::network::SocketAddress& endpoint) override;

private:
    const Settings& m_settings;
    detail::ResolverImpl* m_impl = nullptr;
};

} // namespace nx::geo_ip