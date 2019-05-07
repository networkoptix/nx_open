#include "resolver.h"

#include <nx/utils/log/log.h>

#include "detail/resolver_impl.h"

namespace nx::geo_ip {

Resolver::Resolver(const Settings& settings):
    m_settings(settings)
{
}

Resolver::~Resolver()
{
    try
    {
        if (m_impl)
            delete m_impl;
    }
    catch(const std::exception& e)
    {
        NX_ERROR(this, "error deleting m_impl: %1", e.what());
    }
}

ResultCode Resolver::initialize()
{
    try
    {
        detail::ResolverImpl * impl = new detail::ResolverImpl(m_settings);

        auto resultCode = impl->initialize();
        if (resultCode != ResultCode::ok)
            return resultCode;

        m_impl = impl;
        return ResultCode::ok;
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, "Exception initializing resolver: %1", e.what());
        return ResultCode::unknownError;
    }
}

Result Resolver::resolve(const nx::network::SocketAddress& endpoint)
{
    if (!m_impl)
        return {ResultCode::ioError, {}};

    return m_impl->resolve(endpoint.address.toStdString());
}

} // namespace nx::geo_ip