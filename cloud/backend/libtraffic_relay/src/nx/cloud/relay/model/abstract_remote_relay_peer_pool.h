#pragma once

#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class AbstractRemoteRelayPeerPool
{
public:
    virtual ~AbstractRemoteRelayPeerPool() = default;

    /**
     * NOTE: Can be blocking. But, must return eventually.
     */
    virtual bool connectToDb() = 0;

    virtual bool isConnected() const = 0;

    /**
     * @return Empty string in case of any error.
     */
    virtual void findRelayByDomain(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(std::string /*relay hostname/ip*/)> handler) const = 0;

    virtual void addPeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) = 0;

    virtual void removePeer(
        const std::string& domainName,
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler) = 0;

    virtual void setPublicUrl(const nx::utils::Url& nodeId) = 0;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
