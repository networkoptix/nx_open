/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HTTP_MESSAGE_DISPATCHER_H
#define NX_HTTP_MESSAGE_DISPATCHER_H

#include <memory>

#include "abstract_http_request_handler.h"
#include "http_server_connection.h"
#include <nx/network/connection_server/message_dispatcher.h>
#include "utils/common/cpp14.h"

#define SINGLE_REQUEST_PROCESSOR_PER_REQUEST


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

#ifndef SINGLE_REQUEST_PROCESSOR_PER_REQUEST
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
#else
        template<typename RequestHandlerType>
        bool registerRequestProcessor(
            const QString& path,
            std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc )
        {
            return m_factories.emplace(
                path,
                factoryFunc ).second;
        }

        template<typename RequestHandlerType>
        bool registerRequestProcessor( const QString& path )
        {
            return m_factories.emplace(
                path,
                []()->std::unique_ptr<AbstractHttpRequestHandler>{
                    return std::make_unique<RequestHandlerType>(); } ).second;
        }
#endif
        //!Pass message to corresponding processor
        /*!
            \param message This object is not moved in case of failure to find processor
            \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
        */
        template<class CompletionFuncRefType>
        bool dispatchRequest(
            const HttpServerConnection& conn,
            nx_http::Message&& message,
            stree::ResourceContainer&& authInfo,
            CompletionFuncRefType&& completionFunc )
        {
            assert( message.type == nx_http::MessageType::request );

#ifndef SINGLE_REQUEST_PROCESSOR_PER_REQUEST
            auto it = m_processors.find( message.request->requestLine.url.path() );
            if( it == m_processors.end() )
                return false;
            return it->second->processRequest(
                conn,
                std::move( message ),
                std::forward<CompletionFuncRefType>( completionFunc ) );
#else
            auto it = m_factories.find( message.request->requestLine.url.path() );
            if( it == m_factories.end() )
                return false;
            auto requestProcessor = it->second();
            auto requestProcessorPtr = requestProcessor.release();  //TODO #ak get rid of this when general lambdas available

            return requestProcessorPtr->processRequest(
                conn,
                std::move( message ),
                std::move( authInfo ),
                [completionFunc, requestProcessorPtr](
                    nx_http::Message&& responseMsg,
                    std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody )
                {
                    completionFunc( std::move(responseMsg), std::move(responseMsgBody) );
                    delete requestProcessorPtr;
                } );
#endif
        }

    private:
#ifndef SINGLE_REQUEST_PROCESSOR_PER_REQUEST
        std::map<QString, AbstractHttpRequestHandler*> m_processors;
#else
        std::map<
            QString,
            std::function<std::unique_ptr<AbstractHttpRequestHandler>()>
            > m_factories;
#endif
    };
}

#endif  //NX_HTTP_MESSAGE_DISPATCHER_H
