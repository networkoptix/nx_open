/**********************************************************
* 5 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_REGISTER_HTTP_HANDLER_H
#define NX_REGISTER_HTTP_HANDLER_H

#include <memory>

#include <QString>

#include <utils/network/http/server/abstract_http_request_handler.h>

#include "db/registered_systems_data_manager.h"


class RegisterSystemHttpHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    static const QString HANDLER_PATH;

protected:
    //!Implementation of \a RegisterSystemHttpHandler::AbstractHttpRequestHandler
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override;
};

#endif  //NX_REGISTER_HTTP_HANDLER_H
