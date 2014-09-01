/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_CONNECTOR_H
#define NX_CLOUD_CONNECTOR_H

#include <functional>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Establishes connection to peer by using mediator or existing tunnel
    class CloudConnector
    {
    public:
        virtual ~CloudConnector();

        /*!
            \return \a true if async connection establishing successfully started
        */
        bool requestCloudConnection(
            const HostAddress& hostname,
            std::function<void(nx_cc::ErrorDescription, AbstractStreamSocket*)>&& completionHandler );

        static CloudConnector* instance();
    };
}

#endif  //NX_CLOUD_CONNECTOR_H
