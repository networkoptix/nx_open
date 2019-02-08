#pragma once

#include "abstract_resolver.h"

namespace nx {
namespace network {

/**
 * Resolves address using OS-provided API.
 */
class NX_NETWORK_API SystemResolver:
    public AbstractResolver
{
public:
    virtual SystemError::ErrorCode resolve(
        const QString& hostName,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) override;

private:
    SystemError::ErrorCode resolveStatusToErrno(int status);

    void ensureLocalHostCompatibility(
        const QString& hostName,
        std::deque<AddressEntry>* resolvedAddresses);
};

} // namespace network
} // namespace nx
