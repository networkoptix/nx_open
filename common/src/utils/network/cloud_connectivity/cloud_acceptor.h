/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_ACCEPTOR_H
#define NX_CLOUD_ACCEPTOR_H

#include <functional>

#include "cc_common.h"
#include "../abstract_socket.h"


namespace nx_cc
{
    //!Accepts connections made through mediator
    class CloudAcceptor
    {
    public:
        virtual ~CloudAcceptor();

        bool acceptAsync( std::function<void(const ErrorDescription&, AbstractStreamSocket*)>&& completionHandler );
    };
}

#endif  //NX_CLOUD_ACCEPTOR_H
