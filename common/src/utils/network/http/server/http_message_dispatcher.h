/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HTTP_MESSAGE_DISPATCHER_H
#define NX_HTTP_MESSAGE_DISPATCHER_H

#include "http_server_connection.h"
#include "utils/network/connection_server/message_dispatcher.h"


namespace nx_http
{
    //TODO #ak this class MUST search processor by max string prefix

    typedef std::function<void(
            nx_http::Message&&,
            std::unique_ptr<nx_http::AbstractMsgBodySource> )
        > HttpRequestCompletionFunc;
    typedef std::function<bool(
            const HttpServerConnection&,
            nx_http::Message&&,
            HttpRequestCompletionFunc )
        > HandleHttpRequestFunc;

    class MessageDispatcher
    :
        public
            ::MessageDispatcher<
                HttpServerConnection,
                nx_http::Message,
                typename std::map<QString, HandleHttpRequestFunc>,
                HttpRequestCompletionFunc>

    {
        typedef
            ::MessageDispatcher<
                HttpServerConnection,
                nx_http::Message,
                typename std::map<QString, HandleHttpRequestFunc>,
                HttpRequestCompletionFunc> base_type;
    public:
        MessageDispatcher();
        virtual ~MessageDispatcher();

        //!Pass message to corresponding processor
        /*!
            \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
        */
        template<class CompletionFuncRefType>
        bool dispatchRequest(
            const HttpServerConnection& conn,
            nx_http::Message&& message,
            CompletionFuncRefType&& completionFunc )
        {
            assert( message.type == nx_http::MessageType::request );
            return base_type::dispatchRequest(
                conn,
                message.request->requestLine.url.path(),
                std::move(message),
                std::forward<CompletionFuncRefType>( completionFunc ) );
        }
    };
}

#endif  //NX_HTTP_MESSAGE_DISPATCHER_H
