
#pragma once

#include <utils/common/stoppable.h>

#include "nx/network/cloud/address_resolver.h"
#include "nx/network/socket_attributes_cache.h"
#include "nx/network/socket_common.h"


namespace nx {
namespace network {
namespace cloud {

/** Represents connection established using a nat traversal method (UDP hole punching, tcp hole punching, reverse connection).
    \note Calling party MUST guarantee that no methods of this class are used after 
        \a QnStoppableAsync::pleaseStop call. 
    \note It is allowed to free object right in \a QnStoppableAsync::pleaseStop completion handler
 */
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

    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractStreamSocket>,
        bool /*stillValid*/)> SocketHandler;

    /** Creates new connection to the target peer.
        \note Can return the only connection reporting \a stillValid as \a false 
            in case if real tunneling is not supported by implementation
        \note Implementation MUST guarantee that all handlers are invoked before 
            \a QnStoppableAsync::pleaseStop completion
     */
    virtual void establishNewConnection(
        boost::optional<std::chrono::milliseconds> timeout,
        SocketAttributes socketAttributes,
        SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
    *  \note not all of the AbstractTunnelConnection can really accept connections */
    virtual void accept(SocketHandler handler) = 0;

protected:
    const String m_remotePeerId;
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
     */
    void setStateHandler(std::function<void(State)> handler);

    /** Implementation of QnStoppableAsync::pleaseStop */
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

protected:
    void startConnectors(QnMutexLockerBase* lock);
    void changeState(State state, QnMutexLockerBase* lock);

    typedef std::chrono::system_clock Clock;

    QnMutex m_mutex;
    State m_state;
    std::function<void(State)> m_stateHandler;
    std::shared_ptr<AbstractTunnelConnection> m_connection;
    const String m_remotePeerId;
};

} // namespace cloud
} // namespace network
} // namespace nx
