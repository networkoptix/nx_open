/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_ACCEPTOR_H
#define NX_CLOUD_ACCEPTOR_H

#include <functional>

#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Accepts connections made through mediator
    /*!
        Receives incoming \a connection_requested indications and create cloud tunnel
        \note Uses MediatorConnection to talk to the mediator
    */
    class CloudAcceptor
    :
        public QnStoppableAsync
    {
    public:
        virtual ~CloudAcceptor();

        //!Implementation of QnStoppableAsync::pleaseStop
        virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

        bool acceptAsync( std::function<void(const ErrorDescription&, AbstractStreamSocket*)>&& completionHandler );
    };
}

#endif  //NX_CLOUD_ACCEPTOR_H
