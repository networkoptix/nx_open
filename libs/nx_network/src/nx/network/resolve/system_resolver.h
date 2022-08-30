// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include "abstract_resolver.h"

namespace nx::network {

/**
 * Resolves address using OS-provided API.
 */
class NX_NETWORK_API SystemResolver:
    public AbstractResolver
{
public:
    /**
     * This is a workaround for operating systems where /etc/hosts does not work, thus localhost is
     * inaccessible, like certain Busybox-based systems.
     */
    static void setResolveLocalhost() { s_resolveLocalhost = true; }

    virtual SystemError::ErrorCode resolve(
        const std::string_view& hostName,
        int ipVersion,
        ResolveResult* resolveResult) override;

private:
    static std::atomic<bool> s_resolveLocalhost /*= false*/;

    SystemError::ErrorCode resolveStatusToErrno(int status);

    void ensureLocalHostCompatibility(
        std::deque<AddressEntry>* resolvedAddresses);
};

} // namespace nx::network
