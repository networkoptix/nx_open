/**********************************************************
* 4 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_HTTP_MESSAGE_DISPATCHER_H
#define NX_HTTP_MESSAGE_DISPATCHER_H

#include "http_server_connection.h"
#include "../message_dispatcher.h"


namespace nx_http
{
    //TODO #ak this class MUST search processor by max string prefix

    class MessageDispatcher
    :
        public
            ::MessageDispatcher<
                HttpServerConnection,
                nx_http::Message,
                typename std::map<QString, typename std::function<bool(const std::weak_ptr<HttpServerConnection>&, nx_http::Message&&)> > >

    {
        typedef
            ::MessageDispatcher<
                HttpServerConnection,
                nx_http::Message,
                typename std::map<QString, typename std::function<bool(const std::weak_ptr<HttpServerConnection>&, nx_http::Message&&)> > > base_type;
    public:
        MessageDispatcher();
        virtual ~MessageDispatcher();

        //!Pass message to corresponding processor
        /*!
            \return \a true if request processing passed to corresponding processor and async processing has been started, \a false otherwise
        */
        bool dispatchRequest( const std::weak_ptr<HttpServerConnection>& conn, nx_http::Message&& message )
        {
            assert( message.type == nx_http::MessageType::request );
            return base_type::dispatchRequest( conn, message.request->requestLine.url.path(), std::move(message) );
        }

        static MessageDispatcher* instance();
    };
}

#endif  //NX_HTTP_MESSAGE_DISPATCHER_H
