/**********************************************************
* 5 sep 2014
* a.kolesnikov
***********************************************************/

#include "register_http_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>
#include <utils/network/http/server/http_stream_socket_server.h>


const QString RegisterSystemHttpHandler::HANDLER_PATH = QLatin1String("/register");

void RegisterSystemHttpHandler::processRequest(
    const nx_http::HttpServerConnection& /*connection*/,
    const nx_http::Request& /*request*/,
    nx_http::Response* const response,
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& /*completionHandler*/ )
{
    //RegisteredSystemsDataManager::saveRegistrationDataAsync

    //TODO #ak

    requestDone(
        0,
        nx_http::StatusCode::ok,
        std::make_unique<nx_http::BufferSource>(
            "text/html",
            "<html><h1>Hello, world</h1></html>\n" ) );
}
