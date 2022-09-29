// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <regex>

#include <nx/utils/thread/mutex.h>

#include "../resolve/abstract_resolver.h"

namespace nx::network {

class NX_NETWORK_API CloudAddressResolver:
    public AbstractResolver
{
public:
    CloudAddressResolver();

    virtual SystemError::ErrorCode resolve(
        const std::string_view& hostname,
        int ipVersion,
        ResolveResult* resolveResult) override;

    bool isCloudHostname(const std::string_view& hostname) const;

private:
    const std::regex m_cloudAddressRegex;
    nx::Mutex m_mutex;
};

} // namespace nx::network
