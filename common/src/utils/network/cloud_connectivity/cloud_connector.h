/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_CONNECTOR_H
#define NX_CLOUD_CONNECTOR_H

#include <functional>

#include <utils/common/singleton.h>
#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Establishes connection to peer by using mediator or existing tunnel
    /*!
        \note Uses MediatorConnection to talk to the mediator
    */
    class CloudConnector
    :
        public QnStoppableAsync,
        public Singleton<CloudConnector>
    {
    public:
        virtual ~CloudConnector();

        //!Implementation of QnStoppableAsync::pleaseStop
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        //!Establishes connection to \a targetHost
        /*!
            If there is existing tunnel to \a targetHost, it is used. In other case, new tunnel is established
            \param completionHandler void(nx_cc::ErrorDescription, std::unique_ptr<AbstractStreamSocket>). 
                connection is not \a nullptr only if \a nx_cc::ErrorDescription::resultCode is \a nx_cc::ResultCode::ok
            \return \a true if async connection establishing successfully started
            \note This method call interleaving is allowed even for same host
        */
        template<class Handler>
        bool setupCloudConnection(
            const HostAddress& targetHost,
            Handler&& completionHandler )
        {
            auto tunnel = CloudTunnelPool::instance()->getTunnelToHost( hostname );
            assert( tunnel );
            return tunnel->connect( std::forward<Handler>(completionHandler) );
        }
    };
}

#endif  //NX_CLOUD_CONNECTOR_H
