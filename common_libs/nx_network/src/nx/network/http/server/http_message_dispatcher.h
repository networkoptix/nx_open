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


namespace nx_http
{
    //TODO #ak this class MUST search processor by max string prefix

    typedef std::function<void(
            nx_http::Message&&,
            std::unique_ptr<nx_http::AbstractMsgBodySource> )
        > HttpRequestCompletionFunc;

    class NX_NETWORK_API MessageDispatcher
    {
    public:
        MessageDispatcher();
        virtual ~MessageDispatcher();

        template<typename RequestHandlerType>
        bool registerRequestProcessor(
            const QString& path,
            std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc )
        {
            return m_factories.emplace(
                path,
                std::move(factoryFunc)).second;
        }

        template<typename RequestHandlerType>
        bool registerRequestProcessor( const QString& path )
        {
            return m_factories.emplace(
                path,
                []()->std::unique_ptr<AbstractHttpRequestHandler>{
                    return std::make_unique<RequestHandlerType>(); } ).second;
        }

        template<typename RequestHandlerType>
        void setDefaultProcessor(
            std::function<std::unique_ptr<RequestHandlerType>()> factoryFunc)
        {
            m_defaultHandlerFactory = std::move(factoryFunc);
        }

        template<typename RequestHandlerType>
        void setDefaultProcessor()
        {
            m_defaultHandlerFactory = 
                []()->std::unique_ptr<AbstractHttpRequestHandler>
                {
                    return std::make_unique<RequestHandlerType>();
                };
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
            stree::ResourceContainer&& authInfo,
            CompletionFuncRefType&& completionFunc )
        {
            NX_ASSERT( message.type == nx_http::MessageType::request );

            auto it = m_factories.find( message.request->requestLine.url.path() );
            std::unique_ptr<AbstractHttpRequestHandler> requestProcessor;
            if (it != m_factories.end())
                requestProcessor = it->second();
            else if (m_defaultHandlerFactory)
                requestProcessor = m_defaultHandlerFactory();
            else
                return false;

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
        }

    private:
        std::map<
            QString,
            std::function<std::unique_ptr<AbstractHttpRequestHandler>()>
        > m_factories;
        std::function<std::unique_ptr<AbstractHttpRequestHandler>()> m_defaultHandlerFactory;
    };
}

#endif  //NX_HTTP_MESSAGE_DISPATCHER_H
