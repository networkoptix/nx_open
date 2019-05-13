#include "multicast_parameters.h"

#include <nx/utils/log/log_message.h>

namespace nx::vms::server::resource {

QString MulticastParameters::toString() const
{
    return lm("address: %1. port: %2, ttl: %3").args(address, port, ttl);
}

} // namespace nx::vms::server::resource
