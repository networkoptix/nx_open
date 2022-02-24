// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/resolve/address_entry.h>

namespace nx::network::cloud {

struct TunnelAttributes
{
    AddressType addressType = AddressType::unknown;
    std::string remotePeerName;
};

} // namespace nx::network::cloud
