// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <string_view>

#include <nx/utils/system_error.h>

#include "address_entry.h"
#include "../socket_common.h"

namespace nx {
namespace network {

class AbstractResolver
{
public:
    virtual ~AbstractResolver() = default;

    /**
     * @param resolvedAddresses Not empty in case if SystemError::noError is returned.
     */
    virtual SystemError::ErrorCode resolve(
        const std::string_view& hostName,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) = 0;
};

} // namespace network
} // namespace nx
