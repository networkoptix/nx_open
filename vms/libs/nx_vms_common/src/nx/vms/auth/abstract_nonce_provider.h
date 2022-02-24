// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/string.h>

namespace nx {
namespace vms {
namespace auth {

class AbstractNonceProvider
{
public:
    virtual ~AbstractNonceProvider() = default;

    virtual nx::String generateNonce() = 0;
    virtual bool isNonceValid(const nx::String& nonce) const = 0;
};

} // namespace auth
} // namespace vms
} // namespace nx
