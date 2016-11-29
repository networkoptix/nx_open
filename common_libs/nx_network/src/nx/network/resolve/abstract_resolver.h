#pragma once

#include <deque>

#include <QtCore/QString>

#include "../socket_common.h"

namespace nx {
namespace network {

class AbstractResolver
{
public:
    virtual ~AbstractResolver() = default;

    /**
     * In case of failure, no address is returned and
     * SystemError::getLastOsErrorCode() is set appropriately.
     */
    virtual std::deque<HostAddress> resolve(const QString& hostName, int ipVersion) = 0;
};

} // namespace network
} // namespace nx
