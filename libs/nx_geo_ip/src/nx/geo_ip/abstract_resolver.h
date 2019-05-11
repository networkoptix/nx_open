#pragma once

#include <nx/network/socket_common.h>

#include "types.h"

namespace nx {

namespace geo_ip {

class NX_GEO_IP_API AbstractResolver
{
public:
    virtual ~AbstractResolver() = default;

    /**
     * Throws exception
     */
    virtual Location resolve(const std::string& ipAddress) = 0;
};

}// namespace geo_ip
} // namespace nx
