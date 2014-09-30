/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_CONNECTOR_H
#define NX_CLOUD_CONNECTOR_H

#include <functional>

#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Establishes connection to peer by using mediator or existing tunnel
    class CloudConnector
    :
        public QnStoppableAsync
    {
    public:
        virtual ~CloudConnector();

        //!Implementation of QnStoppableAsync::pleaseStop
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        //!Connects to the specified host whether via existing UDT tunnel or via mediator request
        /*!
            \param completionHandler connection is not \a nullptr only if \a nx_cc::ErrorDescription::resultCode is \a nx_cc::ResultCode::ok
            \return \a true if async connection establishing successfully started
        */
        bool setupCloudConnection(
            const HostAddress& targetHost,
            std::function<void(nx_cc::ErrorDescription, AbstractStreamSocket*)>&& completionHandler );

        static CloudConnector* instance();
    };
}

#endif  //NX_CLOUD_CONNECTOR_H
