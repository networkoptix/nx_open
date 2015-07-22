/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_abstract_http_request_handler_h
#define libcommon_abstract_http_request_handler_h

#include <atomic>
#include <memory>
#include <mutex>

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
            std::function<void(
                nx_http::Message&&,
                std::unique_ptr<nx_http::AbstractMsgBodySource>)> completionHandler );

    protected:
        //!Implement this method to handle request
        /*!
            \param response Implementation is allowed to modify response as it wishes
        */
        virtual void processRequest(
            const nx_http::HttpServerConnection& connection,
            const nx_http::Request& request,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) = 0;

    private:
        class RequestContext
        {
        public:
            nx_http::Message requestMsg;
            nx_http::Message responseMsg;
            std::function<void(
                nx_http::Message&&,
                std::unique_ptr<nx_http::AbstractMsgBodySource>)> completionHandler;

            RequestContext(
                nx_http::Message&& _requestMsg,
                nx_http::Message&& _responseMsg,
                std::function<void(
                    nx_http::Message&&,
                    std::unique_ptr<nx_http::AbstractMsgBodySource>)> _completionHandler )
            :
                requestMsg( std::move( _requestMsg ) ),
                responseMsg( std::move( _responseMsg ) ),
                completionHandler( std::move(_completionHandler) )
            {
            }

            RequestContext( RequestContext&& right )
            :
                requestMsg( std::move( right.requestMsg ) ),
                responseMsg( std::move( right.responseMsg ) ),
                completionHandler( std::move( right.completionHandler ) )
            {
            }

        private:
            RequestContext& operator=( const RequestContext& );
            RequestContext( const RequestContext& );
        };

        size_t m_prevReqSeq;
        std::map<size_t, RequestContext> m_idToReqCtx;
        std::mutex m_mutex;

        void requestDone(
            size_t reqID,
            nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource );
    };
}

#endif  //libcommon_abstract_http_request_handler_h
