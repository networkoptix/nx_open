
#pragma once

#include <utils/common/stoppable.h>

#include "nx/network/cloud/address_resolver.h"
#include "nx/network/socket_attributes_cache.h"
#include "nx/network/socket_common.h"


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
    AbstractTunnelConnection(String remotePeerId)
        : m_remotePeerId(std::move(remotePeerId))
    {
    }

    const String& getRemotePeerId() const { return m_remotePeerId; }

    typedef std::function<void(SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*stillValid*/)> SocketHandler;

    /** Creates new connection to peer or returns current with false in case if real
    *  tunneling is notsupported by used method */
    virtual void connect(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
    *  \note not all of the AbstractTunnelConnection can really accept connections */
    virtual void accept(SocketHandler handler) = 0;

private:
    const String m_remotePeerId;
};

/** Creates outgoing specialized AbstractTunnelConnections */
class AbstractTunnelConnector
:
    public QnStoppableAsync
{
public:
    /** Helps to decide which method shall be used first */
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
        std::function<void(std::unique_ptr<AbstractTunnelConnection>)> handler) = 0;
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
    Tunnel(String remotePeerId);
    Tunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    const String& getRemotePeerId() const { return m_remotePeerId; }

    enum class State { kInit, kConnecting, kConnected, kClosed };
    static QString stateToString(State state);

    /** Creates resposable \class AbstractTunnelConnector if not connected yet */
    //void addConnectionTypes(std::vector<CloudConnectType> type);

    /** Indicates tonnel state */
    void setStateHandler(std::function<void(State)> handler);

    typedef std::function<void(SystemError::ErrorCode,
                               std::unique_ptr<AbstractStreamSocket>)> SocketHandler;
    /** Implementation of QnStoppableAsync::pleaseStop */
    void pleaseStop(std::function<void()> handler) override;


protected:
    void startConnectors(QnMutexLockerBase* lock);
    void changeState(State state, QnMutexLockerBase* lock);

    typedef std::chrono::system_clock Clock;

    QnMutex m_mutex;
    State m_state;
    std::function<void(State)> m_stateHandler;
    std::unique_ptr<AbstractTunnelConnection> m_connection;
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> m_connectors;
    std::multimap<Clock::time_point, SocketHandler> m_connectHandlers;
    const String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
