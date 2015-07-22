/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_base_http_handler_h
#define cloud_db_base_http_handler_h

#include <utils/network/http/server/abstract_http_request_handler.h>
#include <utils/network/http/server/http_server_connection.h>

#include "access_control/types.h"


namespace cdb_api
{
    //!Contains logic common for all cloud_db HTTP request handlers
    class BaseHttpHandler
    :
        public nx_http::AbstractHttpRequestHandler
    {
    public:
        //!Implementation of AbstractHttpRequestHandler::processRequest
        virtual void processRequest(
            const nx_http::HttpServerConnection& connection,
            const nx_http::Request& request,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override;

    protected:
        //!Implement request-specific logic in this function
        virtual void processRequest(
            const AuthorizationInfo& authzInfo,
            const stree::AbstractResourceReader& inputParams,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) = 0;
    };
}

#endif  //cloud_db_base_http_handler_h
