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

    virtual bool connectToDb() = 0;
    /**
     * @return Empty string in case of any error.
     */
    virtual cf::future<std::string /*relay hostname/ip*/> findRelayByDomain(
        const std::string& domainName) const = 0;

    virtual cf::future<bool> addPeer(const std::string& domainName) = 0;
    virtual cf::future<bool> removePeer(const std::string& domainName) = 0;
    virtual void setNodeId(const std::string& nodeId) = 0;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
