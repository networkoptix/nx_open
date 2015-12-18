#ifndef NX_CC_CLOUD_TUNNEL_H
#define NX_CC_CLOUD_TUNNEL_H

#include "address_resolver.h"

namespace nx {
namespace cc {

typedef std::function<void(SystemError::ErrorCode,
                           std::unique_ptr<AbstractStreamSocket>)> SocketHandler;

//TODO #ak there can be AbstractCloudTunnelImpl of different types:
//    - direct or backward tcp connection (no hole punching used)
//    - udt tunnel (no matter hole punching is used or not)
//    - tcp with hole punching
//Last type of tunnel does not allow us to have multiple connections per tunnel (really?)

/** Implements one of cloud connection logics, multiplie certain implementations
 *  will be used by \class CloudTonel to use several connection methods simultiously */
class AbstractCloudTunnelImpl
:
    public QnStoppableAsync
{
public:
    /** Initiates connection using \a address
     *  \a handler with noError indicates that tunnel is successfuly opened
     *  \a handler with error indicates that tunnel was not opend or got closed */
    virtual void open(const SocketAddress& address,
                      std::shared_ptr<StreamSocketOptions> options,
                      std::function<void(SystemError::ErrorCode)> handler) = 0;

    /** Creates new connection to peer or returns current in case if real
     *  tunneling is notsupported by used method */
    virtual void connect(std::shared_ptr<StreamSocketOptions> options,
                         SocketHandler handler) = 0;

    /** Accepts new connection from peer (like socket)
     *  \note not all of the AbstractCloudTunnelImpl can really accept connections */
    virtual void accept(SocketHandler handler) = 0;

    /** Indicates if this tonnel is opened */
    virtual bool isOpen() = 0;
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
class CloudTunnel
:
    public QnStoppableAsync
{
public:
    /** Creates suitable CloudTunnelImpl (if not created yet) */
    void addAddress(const AddressEntry& address);

    /** Establish new connection
     *  \note This method is re-enterable. So, it can be called in
     *        different threads simultaneously */
    void connect(std::shared_ptr<StreamSocketOptions> options, SocketHandler handler);

    /** Accept new incoming connection on the tunnel */
    void accept(SocketHandler handler);

    /** Implementation of QnStoppableAsync::pleaseStop */
    void pleaseStop(std::function<void()> handler) override;

private:
    QnMutex m_mutex;
    SocketHandler m_acceptHandler;
    std::map<AddressEntry, std::unique_ptr<AbstractCloudTunnelImpl>> m_impls;
};

//!Stores cloud tunnels
class CloudTunnelPool
    : public QnStoppableAsync
{
    CloudTunnelPool();
    friend class ::nx::SocketGlobals;

public:
    //! Get tunnel to \a targetHost
    /*!
        Always returns not null tunnel.
        If tunnel does not exist, allocates new tunnel object and returns tunnel object
    */
    std::shared_ptr<CloudTunnel> getTunnel(const String& peerId);

    //! Monitors for all tunnels added
    void monitorTunnels(std::function<void(std::shared_ptr<CloudTunnel>)> handler);

    //! QnStoppableAsync::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

private:
    QnMutex m_mutex;
    std::function<void(std::shared_ptr<CloudTunnel>)> m_monitor;
    std::map<String, std::shared_ptr<CloudTunnel>> m_pool;
    std::shared_ptr< MediatorSystemConnection > m_mediatorConnection;
};

} // namespace cc
} // namespace nx

#endif // NX_CC_CLOUD_TUNNEL_H
