#ifndef NX_CC_CLOUD_ACCEPTOR_H
#define NX_CC_CLOUD_ACCEPTOR_H

#include <functional>

#include <utils/common/stoppable.h>

#include "cc_common.h"
#include "../abstract_socket.h"

namespace nx {
namespace network {
namespace cloud {

//!Accepts connections made through mediator
/*!
    Implements server-side nat traversal logic
    Receives incoming \a connection_requested indications and creates cloud tunnel
    \note Uses MediatorConnection to talk to the mediator
*/
class NX_NETWORK_API CloudAcceptor
:
    public QnStoppableAsync
{
public:
    virtual ~CloudAcceptor();

    //!Implementation of QnStoppableAsync::pleaseStop
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    bool acceptAsync( std::function<void(const ErrorDescription&, AbstractStreamSocket*)>&& completionHandler );
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_CLOUD_ACCEPTOR_H
