#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/utils/counter.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class ListeningPeerPool
{
public:
    using TakeIdleConnection = 
        nx::utils::MoveOnlyFunc<void(api::ResultCode, std::unique_ptr<AbstractStreamSocket>)>;

    ListeningPeerPool();
    ~ListeningPeerPool();

    void addConnection(
        std::string peerName,
        std::unique_ptr<AbstractStreamSocket> connection);

    std::size_t getConnectionCountByPeerName(const std::string& peerName) const;

    bool isPeerListening(const std::string& peerName) const;

    void takeIdleConnection(
        const std::string& peerName,
        TakeIdleConnection completionHandler);

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
    network::aio::BasicPollable m_unsuccessfulResultReporter;
    utils::Counter m_apiCallCounter;

    void giveAwayConnection(
        ListeningPeerPool::ConnectionContext connectionContext,
        ListeningPeerPool::TakeIdleConnection completionHandler);

    void monitoringConnectionForClosure(
        const std::string& peerName,
        ConnectionContext* connectionContext);

    void onConnectionReadCompletion(
        const std::string& peerName,
        ConnectionContext* connectionContext,
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesRead);

    void closeConnection(
        const std::string& peerName,
        ConnectionContext* connectionContext,
        SystemError::ErrorCode sysErrorCode);
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
