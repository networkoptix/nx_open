/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_abstract_http_request_handler_h
#define libcommon_abstract_http_request_handler_h

#include <memory>

#include "http_server_connection.h"
#include "../abstract_msg_body_source.h"


namespace nx_http
{
    //!Base class for all HTTP request processors
    /*!
        \note Class methods are not thread-safe
    */
    class AbstractHttpRequestHandler
    {
    public:
        AbstractHttpRequestHandler();
        virtual ~AbstractHttpRequestHandler();

        /*!
            \param connection This object is valid only in this method. 
                One cannot rely on its availability after return of this method
            \param request Message received
            \param completionHandler Functor to be invoked to send response
        */
        bool processRequest(
            const nx_http::HttpServerConnection& connection,
            nx_http::Message&& request,
            std::function<
                void(
                    nx_http::Message&&,
                    std::unique_ptr<nx_http::AbstractMsgBodySource> )
                > completionHandler );

    protected:
        //!Implement this method to handle request
        virtual void processRequest(
            const nx_http::HttpServerConnection& connection,
            const nx_http::Request& request ) = 0;

        //!Implementation MUST Call this method when done processing request
        void requestDone(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource );
        Response& response();

    private:
        nx_http::Message m_request;
        nx_http::Message m_response;
        std::function<
            void(
                nx_http::Message&&,
                std::unique_ptr<nx_http::AbstractMsgBodySource> )
            > m_completionHandler;
    };
}

#endif  //libcommon_abstract_http_request_handler_h
