#pragma once

#include "abstract_resolver.h"

namespace nx {
namespace network {

class TextIpAddressResolver:
    public AbstractResolver
{
public:
    virtual SystemError::ErrorCode resolve(
        const QString& hostName,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) override;
};

} // namespace network
} // namespace nx
