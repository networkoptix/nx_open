#include "cookie_logout_rest_handler.h"

#include <network/universal_tcp_listener.h>
#include <rest/server/rest_connection_processor.h>

JsonRestResponse QnCookieLogoutRestHandler::executeGet(const JsonRestRequest& request)
{
    logout(request.owner);
    return {nx::network::http::StatusCode::ok};
}

JsonRestResponse QnCookieLogoutRestHandler::executePost(
    const JsonRestRequest& request, const QByteArray& /*body*/)
{
    logout(request.owner);
    return {nx::network::http::StatusCode::ok};
}

void QnCookieLogoutRestHandler::logout(const QnRestConnectionProcessor* connection)
{
    const auto authenticator = QnUniversalTcpListener::authenticator(connection->owner());
    authenticator->removeAccessCookie(connection->request(), connection->response());
}
