#pragma once

#include <string>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class AbstractRemoteRelayPeerPool
{
public:
    virtual ~AbstractRemoteRelayPeerPool() = default;

    virtual cf::future<std::string> findRelayByDomain(const std::string& domainName) const = 0;
    virtual cf::future<bool> addPeer(
        const std::string& domainName,
        const std::string& relayHost) = 0;

    virtual cf::future<bool> removePeer(const std::string& domainName) = 0;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
