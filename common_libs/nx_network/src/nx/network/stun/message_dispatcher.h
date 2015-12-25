/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DISPATCHER_H
#define STUN_MESSAGE_DISPATCHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include <nx/utils/singleton.h>
#include "utils/common/stoppable.h"

#include "server_connection.h"

namespace nx {
namespace stun {

//!Dispatches STUN protocol messages to corresponding processor
/*!
    \note This class methods are not thread-safe
*/
class NX_NETWORK_API MessageDispatcher
{
public:
    typedef std::function<
        void( const std::shared_ptr< ServerConnection >&, stun::Message )
    > MessageProcessor;

    /*!
        \param messageProcessor Ownership of this object is not passed
        \note All processors MUST be registered before connection processing is started,
              since this class methods are not thread-safe
        \return \a true if \a requestProcessor has been registered, \a false otherwise
        \note Message processing function MUST be non-blocking
    */
    bool registerRequestProcessor( int method, MessageProcessor processor );

    //!Pass message to corresponding processor
    /*!
        \param message This object is not moved in case of failure to find processor
        \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
    */
    bool dispatchRequest( const std::shared_ptr< ServerConnection >& connection,
                          stun::Message message ) const;

private:
    std::unordered_map< int, MessageProcessor > m_processors;
};

} // namespace stun
} // namespace nx

#endif  //STUN_MESSAGE_DISPATCHER_H
