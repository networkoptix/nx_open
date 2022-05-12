// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "udt_internet_traffic_metric.h"

#include <nx/network/udt/udt_socket.h>

static auto& statistics = nx::network::UdtStatistics::global;

bool UdtInternetTrafficMetric::isSignificant() const
{
    return nx::network::UdtStatistics::global.internetBytesTransferred != 0;
}

QString UdtInternetTrafficMetric::value() const
{
    return nx::toString(nx::network::UdtStatistics::global.internetBytesTransferred);
}

void UdtInternetTrafficMetric::reset()
{
    nx::network::UdtStatistics::global.internetBytesTransferred = 0;
}
