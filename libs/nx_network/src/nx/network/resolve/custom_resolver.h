// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

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
        const std::string_view& hostName,
        int ipVersion,
        ResolveResult* resolved) override
    {
        return m_func(hostName, ipVersion, resolved);
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
