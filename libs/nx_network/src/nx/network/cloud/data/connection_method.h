// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::hpm::api {

namespace ConnectionMethod {

enum Value
{
    none = 0,
    udpHolePunching = 1,
    tcpHolePunching = 2,
    proxy = 4,
    reverseConnect = 8,
    all = udpHolePunching | tcpHolePunching | proxy | reverseConnect,
};

NX_NETWORK_API std::string toString(int value);

} // namespace ConnectionMethod

using ConnectionMethods = int;

} // namespace nx::hpm::api
