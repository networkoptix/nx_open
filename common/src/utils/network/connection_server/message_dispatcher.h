/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_MESSAGE_DISPATCHER_H
#define NX_MESSAGE_DISPATCHER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include <utils/common/stoppable.h>


//!Dispatches STUN protocol messages to corresponding processor
/*!
    \param ProcessorDictionaryType Maps \a key_type to \a MessageProcessorType

    \note This class methods are not thread-safe
    \note Message processing function MUST be non-blocking
*/
template<
    class ConnectionType,
    class MessageType,
    class ProcessorDictionaryType>
class MessageDispatcher
:
    public QnStoppableAsync
{
public:
    typedef ConnectionType connection_type;
    typedef MessageType message_type;
    typedef ProcessorDictionaryType processor_dictionary_type;

    //!Message processor functor type
    /*!
        \note It is guaranteed that connection cannot be freed while in processor function. But, it can be closed at any moment after return of processor.
            To receive connection close event use \a BaseServerConnection::registerCloseHandler.
            If handler has to save connection for later use (e.g., handler uses some async fsm) than it is strongly recommended to save weak pointer to connection, 
            not strong one! And register connection closure handler with \a BaseServerConnection::registerCloseHandler
    */
    typedef std::function<bool(connection_type*, message_type&&)> MessageProcessorType;

    //!Implementation of \a QnStoppableAsync::pleaseStop
    /*!
        Stop dispatching requests. \a StunRequestDispatcher::dispatchRequest returns \a false if dispatcher has been stopped
        \note request dispatching cannot be resumed after it has been stopped
    */
    virtual void pleaseStop( std::function<void()>&& /*completionHandler*/ ) override
    {
        //TODO #ak
    }

    /*!
        \param messageProcessor Ownership of this object is not passed
        \note All processors MUST be registered before connection processing is started, since this class methods are not thread-safe
        \return \a true if \a requestProcessor has been registered, \a false otherwise
        \note Message processing function MUST be non-blocking
    */
    bool registerRequestProcessor( typename ProcessorDictionaryType::key_type method, MessageProcessorType&& messageProcessor )
    {
        return m_processors.emplace( method, messageProcessor ).second;
    }
    //!Pass message to corresponding processor
    /*!
        \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
    */
    bool dispatchRequest(
        connection_type* conn,
        typename ProcessorDictionaryType::key_type method,
        message_type&& message )
    {
        auto it = m_processors.find( method );
        if( it == m_processors.end() )
            return false;
        return it->second( conn, std::move(message) );
    }

private:
    ProcessorDictionaryType m_processors;
};

#endif  //NX_MESSAGE_DISPATCHER_H
