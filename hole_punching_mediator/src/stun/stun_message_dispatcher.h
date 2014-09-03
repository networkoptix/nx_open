/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DISPATCHER_H
#define STUN_MESSAGE_DISPATCHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include <utils/common/stoppable.h>

#include "../stun_server_connection.h"


//!Dispatches STUN protocol messages to corresponding processor
/*!
    \note This class methods are not thread-safe
*/
class STUNMessageDispatcher
:
    public QnStoppableAsync
{
public:
    typedef std::function<bool(const std::weak_ptr<StunServerConnection>&, nx_stun::Message&&)> MessageProcessorType;

    STUNMessageDispatcher();
    virtual ~STUNMessageDispatcher();

    //!Implementation of \a QnStoppableAsync::pleaseStop
    /*!
        Stop dispatching requests. \a StunRequestDispatcher::dispatchRequest will returns \a false
        \note request dispatching cannot be resumed after it has been stopped
    */
    virtual void pleaseStop( std::function<void()>&& completionHandler ) override;

    /*!
        \param messageProcessor Ownership of this object is not passed
        \note All processors MUST be registered before connection processing is started, since this class methods are not thread-safe
        \return \a true if \a requestProcessor has been registered, \a false otherwise
    */
    bool registerRequestProcessor( int method, MessageProcessorType&& messageProcessor );
    //!Pass message to corresponding processor
    /*!
        \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
    */
    bool dispatchRequest( const std::weak_ptr<StunServerConnection>& conn, nx_stun::Message&& message );

    static STUNMessageDispatcher* instance();

private:
    std::unordered_map<int, MessageProcessorType> m_processors;
};

#endif  //STUN_MESSAGE_DISPATCHER_H
