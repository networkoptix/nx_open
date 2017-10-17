#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

#include "settings.h"

namespace nx {
namespace cloud {
namespace relaying {

using TakeIdleConnectionHandler = nx::utils::MoveOnlyFunc<
    void(relay::api::ResultCode, std::unique_ptr<AbstractStreamSocket>)>;

struct ClientInfo
{
    std::string relaySessionId;
    SocketAddress endpoint;
    std::string peerName;
};

class NX_RELAYING_API AbstractListeningPeerPool
{
public:
    virtual ~AbstractListeningPeerPool() = default;

    virtual void addConnection(
        const std::string& peerName,
        std::unique_ptr<AbstractStreamSocket> connection) = 0;

    virtual std::size_t getConnectionCountByPeerName(const std::string& peerName) const = 0;

    /**
     * Peer is online if:
     * - There are active connections.
     * - Or emptyPoolTimeout has not passed since loss of the last connection.
     */
    virtual bool isPeerOnline(const std::string& peerName) const = 0;

    /**
     * E.g., if we have peers server1.nx.com and server2.nx.com then
     * findListeningPeerByDomain("nx.com") will return any of that peers.
     * At the same time findListeningPeerByDomain("server1.nx.com") will return server1.nx.com.
     * @return Empty string if nothing found.
     */
    virtual std::string findListeningPeerByDomain(const std::string& domainName) const = 0;

    /**
     * If peerName is not known, then api::ResultCode::notFound is reported.
     * If peer is found, but no connections from it at the moment, then it will wait
     * for some timeout for peer to establish new connection.
     */
    virtual void takeIdleConnection(
        const ClientInfo& clientInfo,
        const std::string& peerName,
        TakeIdleConnectionHandler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_RELAYING_API ListeningPeerPool:
    public AbstractListeningPeerPool
{
public:
    ListeningPeerPool(const Settings& settings);
    ~ListeningPeerPool();

    virtual void addConnection(
        const std::string& peerName,
        std::unique_ptr<AbstractStreamSocket> connection) override;

    virtual std::size_t getConnectionCountByPeerName(
        const std::string& peerName) const override;

    virtual bool isPeerOnline(const std::string& peerName) const override;

    virtual std::string findListeningPeerByDomain(
        const std::string& domainName) const override;

    virtual void takeIdleConnection(
        const ClientInfo& clientInfo,
        const std::string& peerName,
        TakeIdleConnectionHandler completionHandler) override;

    nx::utils::Subscription<std::string /*peer full name*/>& peerConnectedSubscription();
    nx::utils::Subscription<std::string /*peer full name*/>& peerDisconnectedSubscription();

private:
    /**
     * Peer becomes "disconnected" when there are no idle connections.
     */
    using DisconnectedPeerExpirationTimers = 
        std::multimap<std::chrono::steady_clock::time_point, std::string>;

    using TakeIdleConnectionRequestTimers = 
        std::multimap<std::chrono::steady_clock::time_point, std::string>;

    struct ConnectionContext
    {
        std::unique_ptr<AbstractStreamSocket> connection;
        nx::Buffer readBuffer;
    };

    struct ConnectionAwaitContext
    {
        ClientInfo clientInfo;
        std::chrono::steady_clock::time_point expirationTime;
        TakeIdleConnectionHandler handler;
        TakeIdleConnectionRequestTimers::iterator expirationTimerIter;
    };
    
    struct PeerContext
    {
        std::deque<std::unique_ptr<ConnectionContext>> connections;
        boost::optional<DisconnectedPeerExpirationTimers::iterator> expirationTimer;
        std::list<ConnectionAwaitContext> takeConnectionRequestQueue;
    };

    /** multimap<full peer name, connection context> */
    using PeerConnections = std::map<std::string, PeerContext>;

    const Settings& m_settings;
    PeerConnections m_peers;
    mutable QnMutex m_mutex;
    bool m_terminated;
    network::aio::BasicPollable m_unsuccessfulResultReporter;
    utils::Counter m_apiCallCounter;
    DisconnectedPeerExpirationTimers m_peerExpirationTimers;
    nx::network::aio::Timer m_periodicTasksTimer;
    TakeIdleConnectionRequestTimers m_takeIdleConnectionRequestTimers;
    nx::utils::Subscription<std::string> m_peerConnectedSubscription;
    nx::utils::Subscription<std::string> m_peerDisconnectedSubscription;
    std::deque<nx::utils::MoveOnlyFunc<void()>> m_scheduledEvents;

    bool someoneIsWaitingForPeerConnection(const PeerContext& peerContext) const;
    void provideConnectionToTheClient(
        const QnMutexLockerBase& lock,
        const std::string& peerName,
        PeerContext* peerContext,
        std::unique_ptr<ConnectionContext> connectionContext);

    void startWaitingForNewConnection(
        const QnMutexLockerBase& lock,
        const ClientInfo& clientInfo,
        const std::string& peerName,
        PeerContext* peerContext,
        TakeIdleConnectionHandler completionHandler);

    void giveAwayConnection(
        const ClientInfo& clientInfo,
        std::unique_ptr<ConnectionContext> connectionContext,
        TakeIdleConnectionHandler completionHandler);

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

    void startPeerExpirationTimer(
        const QnMutexLockerBase& lock,
        const std::string& peerName,
        PeerContext* peerContext);
    void processExpirationTimers(
        const QnMutexLockerBase& lock,
        std::chrono::steady_clock::time_point currentTime = nx::utils::monotonicTime());
    void processExpirationTimers(std::chrono::steady_clock::time_point currentTime);

    void doPeriodicTasks();
    void handleTimedoutTakeConnectionRequests(
        const QnMutexLockerBase& lock,
        std::chrono::steady_clock::time_point currentTime);
    std::vector<TakeIdleConnectionHandler> findTimedoutTakeConnectionRequestHandlers(
        const QnMutexLockerBase& lock,
        std::chrono::steady_clock::time_point currentTime);

    void scheduleEvent(nx::utils::MoveOnlyFunc<void()> raiseEventFunc);
    void forcePeriodicTasksProcessing();
    void raiseScheduledEvents();
};

//-------------------------------------------------------------------------------------------------

using ListeningPeerPoolFactoryFunction =
    std::unique_ptr<AbstractListeningPeerPool>(const Settings& /*settings*/);

class NX_RELAYING_API ListeningPeerPoolFactory:
    public nx::utils::BasicFactory<ListeningPeerPoolFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ListeningPeerPoolFactoryFunction>;

public:
    ListeningPeerPoolFactory();

    static ListeningPeerPoolFactory& instance();

private:
    std::unique_ptr<AbstractListeningPeerPool> defaultFactoryFunction(
        const Settings& settings);
};

} // namespace relay
} // namespace cloud
} // namespace nx
