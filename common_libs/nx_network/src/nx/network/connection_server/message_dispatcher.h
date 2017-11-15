#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <nx/network/async_stoppable.h>

namespace nx {
namespace network {
namespace server {

//!Dispatches STUN protocol messages to corresponding processor
/*!
    \param ProcessorDictionaryType Maps key_type to MessageProcessorType

    NOTE: This class methods are not thread-safe
    NOTE: Message processing function MUST be non-blocking
*/
template<
    class ConnectionType,
    class MessageType,
    class ProcessorDictionaryType,
    class CompletionFuncType>
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
        NOTE: It is guaranteed that connection cannot be freed while in processor function. But, it can be closed at any moment after return of processor.
            To receive connection close event use BaseServerConnection::registerCloseHandler.
            If handler has to save connection for later use (e.g., handler uses some async fsm) than it is strongly recommended to save weak pointer to connection, 
            not strong one! And register connection closure handler with BaseServerConnection::registerCloseHandler
    */
    typedef std::function<bool(
            const connection_type&,
            message_type&&,
            CompletionFuncType )
        > MessageProcessorType;

    //!Implementation of QnStoppableAsync::pleaseStop
    /*!
        Stop dispatching requests. StunRequestDispatcher::dispatchRequest returns false if dispatcher has been stopped
        NOTE: request dispatching cannot be resumed after it has been stopped
    */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> /*completionHandler*/) override
    {
        //TODO #ak
    }

    /*!
        \param messageProcessor Ownership of this object is not passed
        NOTE: All processors MUST be registered before connection processing is started, since this class methods are not thread-safe
        \return true if requestProcessor has been registered, false otherwise
        NOTE: Message processing function MUST be non-blocking
    */
    bool registerRequestProcessor( typename ProcessorDictionaryType::key_type method, MessageProcessorType&& messageProcessor )
    {
        return m_processors.emplace( method, messageProcessor ).second;
    }
    //!Pass message to corresponding processor
    /*!
        \param message This object is not moved in case of failure to find processor
        \return true if request processing passed to corresponding processor and async processing has been started, false otherwise
    */
    template<class CompletionFuncRefType>
    bool dispatchRequest(
        const connection_type& conn,
        typename ProcessorDictionaryType::key_type method,
        message_type&& message,
        CompletionFuncRefType&& completionFunc )
    {
        auto it = m_processors.find( method );
        if( it == m_processors.end() )
            return false;
        return it->second(
            conn,
            std::move(message),
            std::forward<CompletionFuncRefType>(completionFunc) );
    }

private:
    ProcessorDictionaryType m_processors;
};

} // namespace server
} // namespace network
} // namespace nx
