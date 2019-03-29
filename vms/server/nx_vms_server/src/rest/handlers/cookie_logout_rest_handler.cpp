#include "cookie_logout_rest_handler.h"

#include <network/universal_tcp_listener.h>
#include <rest/server/rest_connection_processor.h>

RestResponse QnCookieLogoutRestHandler::executeGet(const RestRequest& request)
{
    logout(request.owner);
    return {nx::network::http::StatusCode::ok};
}

RestResponse QnCookieLogoutRestHandler::executePost(const RestRequest& request)
{
    logout(request.owner);
    return {nx::network::http::StatusCode::ok};
}

void QnCookieLogoutRestHandler::logout(const QnRestConnectionProcessor* connection)
{
    const auto authenticator = QnUniversalTcpListener::authenticator(connection->owner());
    authenticator->removeAccessCookie(connection->request(), connection->response());
}
