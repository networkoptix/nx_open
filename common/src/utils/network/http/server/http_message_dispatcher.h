/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HTTP_MESSAGE_DISPATCHER_H
#define NX_HTTP_MESSAGE_DISPATCHER_H

#include "abstract_http_request_handler.h"
#include "http_server_connection.h"
#include "utils/network/connection_server/message_dispatcher.h"


namespace nx_http
{
    //TODO #ak this class MUST search processor by max string prefix

    typedef std::function<void(
            nx_http::Message&&,
            std::unique_ptr<nx_http::AbstractMsgBodySource> )
        > HttpRequestCompletionFunc;

    class MessageDispatcher
    {
    public:
        MessageDispatcher();
        virtual ~MessageDispatcher();

        /*!
            \param messageProcessor Ownership of this object is not passed
            \note All processors MUST be registered before connection processing is started, since this class methods are not thread-safe
            \return \a true if \a requestProcessor has been registered, \a false otherwise
            \note Message processing function MUST be non-blocking
        */
        bool registerRequestProcessor(
            const QString& path,
            AbstractHttpRequestHandler* messageProcessor )
        {
            return m_processors.emplace( path, messageProcessor ).second;
        }

        //!Pass message to corresponding processor
        /*!
            \param message This object is not moved in case of failure to find processor
            \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
        */
        template<class CompletionFuncRefType>
        bool dispatchRequest(
            const HttpServerConnection& conn,
            nx_http::Message&& message,
            CompletionFuncRefType&& completionFunc )
        {
            assert( message.type == nx_http::MessageType::request );

            auto it = m_processors.find( message.request->requestLine.url.path() );
            if( it == m_processors.end() )
                return false;
            return it->second->processRequest(
                conn,
                std::move( message ),
                std::forward<CompletionFuncRefType>( completionFunc ) );
        }

    private:
        std::map<QString, AbstractHttpRequestHandler*> m_processors;
    };
}

#endif  //NX_HTTP_MESSAGE_DISPATCHER_H
