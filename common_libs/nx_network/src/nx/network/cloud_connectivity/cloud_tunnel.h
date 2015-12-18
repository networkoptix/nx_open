/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_TUNNEL_H
#define NX_CLOUD_TUNNEL_H

#include <functional>
#include <list>
#include <memory>

#include <nx/utils/singleton.h>
#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "address_resolver.h"
#include "../abstract_socket.h"

#include <QMutex>

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
                      std::function<void(SystemError::ErrorCode)> handler) = 0;

    /** Creates new connection to peer or returns current in case if real
     *  tunneling is notsupported by used method */
    virtual void connect(SocketHandler handler) = 0;

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
    //! Implementation of QnStoppableAsync::pleaseStop
    virtual void pleaseStop(std::function<void()> handler) override
    {
        // TODO: call pleaseStop on all internal AbstractCloudTunnelImpls
    }

    /** Saves \a address and initiates and creates certain new CloudTunnelImpl
     *  if this tunnel has not any connected CloudTunnelImpls
     *  \note address can also be used later to create another CloudTunnelImpl
     *        in case if current gets broken */
    void addAddress(const AddressEntry& address)
    {
        // TODO: create certain AbstractCloudTunnelImpl and open is if there are
        //       on other already connected tunnels
    }

    //!Establish new connection
    /*!
        \param timeoutMillis
        \param handler. void(ErrorDescription, std::unique_ptr<AbstractStreamSocket>)
        \note This method is re-enterable. So, it can be called in different threads simultaneously
        \note Request interleaving is allowed
    */
    void connect(unsigned int timeoutMillis, SocketHandler handler)
    {
        // TODO: if there some opened AbstractCloudTunnelImpl delegate connect
        //       to it, otherwise try to open all underlaing AbstractCloudTunnelImpls
        //       and delegate connect to the first successfuly opend
    }

    //!Accept new incoming connection
    /*!
        \param handler. void(const ErrorDescription&, std::unique_ptr<AbstractStreamSocket>)
        //TODO #ak who listenes tunnel?
    */
    void accept(SocketHandler handler)
    {
        // TODO: accepts connections from all AbstractCloudTunnelImpls
    }

private:
    std::map<AddressEntry, std::unique_ptr<AbstractCloudTunnelImpl>> m_impls;
};

/** Listens for stun indications and creates new CloudTunnels in CloudTunnelPool.
 *  this class is supposed to be instantiated in CloudServerSocket */
class CloudTunnelAcceptor
{
};

//!Stores cloud tunnels
class CloudTunnelPool
:
    public Singleton<CloudTunnelPool>
{
public:
    //!Get tunnel to \a targetHost
    /*!
        Always returns not null tunnel.
        If tunnel does not exist, allocates new tunnel object and returns tunnel object
    */
    std::shared_ptr<CloudTunnel> getTunnel(const String& peerId);

    //! Monitors for all tunnels added
    void monitorTunnels(std::function<void(std::shared_ptr<CloudTunnel>)> handler);

private:
    QMutex m_mutex;
    std::function<void(std::shared_ptr<CloudTunnel>)> m_monitor;
    std::map<String, std::shared_ptr<CloudTunnel>> m_pool;
};

} // namespace cc
} // namespace nx

#endif  //NX_CLOUD_TUNNEL_H
