#pragma once

#include <deque>

#include <QtCore/QString>

#include <nx/utils/system_error.h>

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
        const QString& hostName,
        int ipVersion,
        std::deque<HostAddress>* resolvedAddresses) = 0;
};

} // namespace network
} // namespace nx
