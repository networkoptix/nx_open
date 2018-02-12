#pragma once

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "abstract_resolver.h"

namespace nx {
namespace network {

template<typename Func>
class CustomResolver:
    public AbstractResolver
{
public:
    CustomResolver(Func func):
        m_func(std::move(func))
    {
    }

    virtual SystemError::ErrorCode resolve(
        const QString& hostName,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) override
    {
        return m_func(hostName, ipVersion, resolvedAddresses);
    }

private:
    Func m_func;
};

template<typename Func>
static std::unique_ptr<CustomResolver<Func>> makeCustomResolver(Func func)
{
    return std::make_unique<CustomResolver<Func>>(std::move(func));
}

} // namespace network
} // namespace nx
