#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class ListeningPeerPool
{
public:
    ListeningPeerPool();
    ~ListeningPeerPool();

    void addConnection(
        std::string peerName,
        std::unique_ptr<AbstractStreamSocket> connection);

    std::size_t getConnectionCountByPeerName(const std::string& peerName) const;

private:
    struct ConnectionContext
    {
        std::unique_ptr<AbstractStreamSocket> connection;
        nx::Buffer readBuffer;
    };
    
    using PeerConnections = std::multimap<std::string, ConnectionContext>;

    PeerConnections m_peerNameToConnection;
    mutable QnMutex m_mutex;
    bool m_terminated;

    void monitoringConnectionForClosure(PeerConnections::iterator);

    void onConnectionReadCompletion(
        ListeningPeerPool::PeerConnections::iterator connectionIter,
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesRead);

    void closeConnection(
        ListeningPeerPool::PeerConnections::iterator connectionIter,
        SystemError::ErrorCode sysErrorCode);
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
