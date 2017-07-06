#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class RemoteRelayPeerPool
{
public:
    std::string findListeningPeerByDomain(const std::string& domainName) const;
    bool addPeer(const std::string& domainName, const std::string& peerName);

private:
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
