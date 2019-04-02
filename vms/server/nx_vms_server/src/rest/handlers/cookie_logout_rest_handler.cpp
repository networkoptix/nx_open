#include "cookie_logout_rest_handler.h"

#include <network/universal_tcp_listener.h>
#include <rest/server/rest_connection_processor.h>

using namespace nx::network;

rest::Response QnCookieLogoutRestHandler::executeGet(const rest::Request& request)
{
    logout(request.owner);
    return {http::StatusCode::ok};
}

rest::Response QnCookieLogoutRestHandler::executePost(const rest::Request& request)
{
    logout(request.owner);
    return {http::StatusCode::ok};
}

void QnCookieLogoutRestHandler::logout(const QnRestConnectionProcessor* connection)
{
    const auto authenticator = QnUniversalTcpListener::authenticator(connection->owner());
    authenticator->removeAccessCookie(connection->request(), connection->response());
}
