#include "udt_internet_traffic_metric.h"

#include <nx/network/udt/udt_socket.h>

static auto& statistics = nx::network::UdtStatistics::global;

bool UdtInternetTrafficMetric::isSignificant() const
{
    return nx::network::UdtStatistics::global.internetBytesTransfered != 0;
}

QString UdtInternetTrafficMetric::value() const
{
    return toString(nx::network::UdtStatistics::global.internetBytesTransfered);
}

void UdtInternetTrafficMetric::reset()
{
    nx::network::UdtStatistics::global.internetBytesTransfered = 0;
}
