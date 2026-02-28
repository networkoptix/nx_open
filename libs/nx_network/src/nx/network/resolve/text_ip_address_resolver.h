// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_resolver.h"

namespace nx::network {

class TextIpAddressResolver:
    public AbstractResolver
{
public:
    virtual SystemError::ErrorCode resolve(
        std::string_view hostName,
        int ipVersion,
        ResolveResult* resolveResult) override;
};

} // namespace nx::network
