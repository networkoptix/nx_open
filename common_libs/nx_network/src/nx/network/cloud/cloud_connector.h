#ifndef NX_CC_CLOUD_CONNECTOR_H
#define NX_CC_CLOUD_CONNECTOR_H

#include <functional>

#include <nx/utils/singleton.h>
#include <utils/common/stoppable.h>
#include <nx/network/cloud/cloud_tunnel.h>

#include "cc_common.h"
#include "../abstract_socket.h"

namespace nx {
namespace network {
namespace cloud {

//!Establishes connection to peer by using mediator
/*!
    Implements client-side nat traversal logic
    \note Uses MediatorConnection to talk to the mediator
*/
class NX_NETWORK_API CloudConnector
:
    public QnStoppableAsync,
    public Singleton<CloudConnector>    //really singleton?
{
public:
    virtual ~CloudConnector();

    //!Implementation of QnStoppableAsync::pleaseStop
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    //!Establishes connection to \a targetHost
    /*!
        If there is existing tunnel to \a targetHost, it is used. In other case, new tunnel is established
        \param completionHandler void(ErrorDescription, std::unique_ptr<AbstractStreamSocket>).
            connection is not \a nullptr only if \a ErrorDescription::resultCode is \a ResultCode::ok
        \return \a true if async connection establishing successfully started
        \note This method call interleaving is allowed even for same host
    */
    template<class Handler>
    bool setupCloudConnection(
        const HostAddress& targetHost,
        Handler&& completionHandler )
    {
        auto tunnel = TunnelPool::instance()->getTunnelToHost( targetHost );
        NX_ASSERT( tunnel );
        return tunnel->connect( std::forward<Handler>(completionHandler) );
    }
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_CONNECTOR_H
