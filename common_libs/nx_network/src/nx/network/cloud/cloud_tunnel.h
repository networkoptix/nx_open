#ifndef NX_CC_CLOUD_TUNNEL_H
#define NX_CC_CLOUD_TUNNEL_H

#include "address_resolver.h"

#include <queue>

namespace nx {
namespace network {
namespace cloud {

//TODO #ak there can be AbstractAbstractTunnelConnection of different types:
//    - direct or backward tcp connection (no hole punching used)
//    - udt tunnel (no matter hole punching is used or not)
//    - tcp with hole punching
//Last type of tunnel does not allow us to have multiple connections per tunnel (really?)

/** Implements one of cloud connection logics, multiplie certain implementations
 *  will be used by \class CloudTonel to use several connection methods simultiously */
class AbstractTunnelConnection
:
    public QnStoppableAsync
{
public:
    typedef std::function<void(SystemError::ErrorCode,
                               std::unique_ptr<AbstractStreamSocket>,
                               bool /*stillValid*/)> SocketHandler;

    /** Creates new connection to peer or returns current with false in case if real
     *  tunneling is notsupported by used method */
    virtual void connect(std::chrono::milliseconds timeout,
                         SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
     *  \note not all of the AbstractTunnelConnection can really accept connections */
    virtual void accept(SocketHandler handler) = 0;
};

/** Creates outgoing specialized AbstractTunnelConnections */
class AbstractTunnelConnector
:
    public QnStoppableAsync
{
public:
    /** Helps to decide which method shell be used first */
    virtual uint32_t getPriority() = 0;

    /** Creates connected AbstractTunnelConnection */
    virtual void connect(
        std::function<void(std::unique_ptr<AbstractTunnelConnection>)> handler) = 0;
};

/** Creates incoming specialized AbstractTunnelConnections */
class AbstractTunnelAcceptor
:
    public QnStoppableAsync
{
public:
    virtual void accept(
        std::function<void(String,
                           std::unique_ptr<AbstractTunnelConnection>)> handler) = 0;
};

//!Tunnel between two peers. Tunnel can be established by backward TCP connection or by hole punching UDT connection
/*!
    Tunnel connection establishment is outside of scope of this class.
    Instance of this class does not re-establish connection if it has been broken since connection establishment may
        require some complicated steps (like going through mediator, nat traversal)
    \note It is full-duplex tunnel
    \note We use tunnel to reduce load on mediator
    \note Implements logic of existing tunnel:\n
        - keep-alive messages
        - setting up (connecting and accepting) more connections via existing tunnel
        - reconnecting (with help of \a CloudTunnelConnector or \a CloudTunnelAcceptor) in case of connection problems
    \note can create connection without mediator if tunnel is:\n
        - direct or backward tcp connection (no hole punching used)
        - udt tunnel (no matter hole punching is used or not)
    \note Methods are thread-safe. I.e., different threads can use this tunnel to establish connection
*/
class Tunnel
:
    public QnStoppableAsync
{
public:
    Tunnel(String peerId);

    enum class State { kInit, kConnecting, kConnected, kClosed };
    static QString stateToString(State state);

    /** Creates resposable \class AbstractTunnelConnector if not connected yet */
    void addConnectionTypes(std::vector<CloudConnectType> type);

    /** Addopts external connection */
    void adoptConnection(std::unique_ptr<AbstractTunnelConnection> connection);

    /** Indicates tonnel state */
    void setStateHandler(std::function<void(State)> handler);

    typedef std::function<void(SystemError::ErrorCode,
                               std::unique_ptr<AbstractStreamSocket>)> SocketHandler;

    /** Establish new connection
     *  \note This method is re-enterable. So, it can be called in
     *        different threads simultaneously */
    void connect(std::chrono::milliseconds timeout,
                 SocketHandler handler);

    /** Accept new incoming connection on the tunnel */
    void accept(SocketHandler handler);

    /** Implementation of QnStoppableAsync::pleaseStop */
    void pleaseStop(std::function<void()> handler) override;

private:
    void startConnectors(QnMutexLockerBase* lock);
    void changeState(State state, QnMutexLockerBase* lock);

    const String m_peerId;
    typedef std::chrono::system_clock Clock;

    QnMutex m_mutex;
    State m_state;
    std::function<void(State)> m_stateHandler;
    std::unique_ptr<AbstractTunnelConnection> m_connection;
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> m_connectors;
    std::multimap<Clock::time_point, SocketHandler> m_connectHandlers;
};

//!Stores cloud tunnels
class TunnelPool
    : public QnStoppableAsync
{
public:
    TunnelPool();

    void connect(const String& peerId,
                 std::vector<CloudConnectType> connectTypes,
                 std::shared_ptr<StreamSocketOptions> options,
                 Tunnel::SocketHandler handler);

    void accept(std::shared_ptr<StreamSocketOptions> options,
                Tunnel::SocketHandler handler);

    //! QnStoppableAsync::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

private:
    std::shared_ptr<Tunnel> getTunnel(const String& peerId);
    void acceptTunnel(const String& peerId);
    void removeTunnel(const String& peerId);

    struct SocketRequest
    {
        std::shared_ptr<StreamSocketOptions> options;
        Tunnel::SocketHandler handler;
    };

    void waitConnectingSocket(std::unique_ptr<AbstractStreamSocket> socket,
                              SocketRequest request);

    void indicateAcceptedSocket(std::unique_ptr<AbstractStreamSocket> socket,
                                SocketRequest request);

    QnMutex m_mutex;
    std::map<String, std::shared_ptr<Tunnel>> m_pool;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    boost::optional<SocketRequest> m_acceptRequest;
    std::queue<std::unique_ptr<AbstractStreamSocket>> m_acceptedSockets;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_H
