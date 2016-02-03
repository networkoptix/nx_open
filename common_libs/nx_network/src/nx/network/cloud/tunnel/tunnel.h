
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

    typedef std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*tunnelStillValid*/)> SocketHandler;

    /** Creates new connection to peer or returns current with false in case if real
    *  tunneling is notsupported by used method */
    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
    *  \note not all of the AbstractTunnelConnection can really accept connections */
    virtual void accept(SocketHandler handler) = 0;

private:
    const String m_remotePeerId;
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
    \note Methods are thread-safe. I.e., different threads can use this tunnel to establish connection
*/
class Tunnel
:
    public QnStoppableAsync
{
public:
    typedef std::function<void(SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>)> SocketHandler;

    enum class State
    {
        kInit,
        kConnecting,
        kConnected,
        kClosed
    };
    static QString stateToString(State state);

    Tunnel(String remotePeerId);
    Tunnel(std::unique_ptr<AbstractTunnelConnection> connection);

    const String& getRemotePeerId() const { return m_remotePeerId; }

    /** Indicates tunnel state.
        \note It is allowed to free \a Tunnel in \a State::kClosed handler
     */
    void setStateHandler(std::function<void(State)> handler);

    /** Implementation of QnStoppableAsync::pleaseStop */
    //void pleaseStop(std::function<void()> handler) override;


    // TODO: replace with actual accept message
    static const QByteArray ACCEPT_INDICATION;

protected:
    void startConnectors(QnMutexLockerBase* lock);
    void changeState(State state, QnMutexLockerBase* lock);

    typedef std::chrono::system_clock Clock;

    QnMutex m_mutex;
    State m_state;
    std::function<void(State)> m_stateHandler;
    std::unique_ptr<AbstractTunnelConnection> m_connection;
    const String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
