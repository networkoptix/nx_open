// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_method.h"

#include <array>

#include <nx/utils/string.h>

namespace nx::hpm::api::ConnectionMethod {

std::string toString(int value)
{
    std::array<std::string_view, 4> strs;
    std::size_t count = 0;

    if (value & udpHolePunching)
        strs[count++] = "udpHolePunching";
    if (value & tcpHolePunching)
        strs[count++] = "tcpHolePunching";
    if (value & proxy)
        strs[count++] = "proxy";
    if (value & reverseConnect)
        strs[count++] = "reverseConnect";

    return nx::utils::join(strs.begin(), strs.begin() + count, ',');
}

} // namespace nx::hpm::api::ConnectionMethod
