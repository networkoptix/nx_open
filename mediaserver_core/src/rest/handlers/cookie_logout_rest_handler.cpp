#include "cookie_logout_rest_handler.h"

#include <network/universal_tcp_listener.h>
#include <rest/server/rest_connection_processor.h>

int QnCookieLogoutRestHandler::executeGet(
    const QString &, const QnRequestParams &/*params*/,
    QnJsonRestResult &/*result*/, const QnRestConnectionProcessor* owner)
{
    QnUniversalTcpListener::authenticator(owner->owner())
        ->removeAccessCookie(owner->request(), owner->response());

    return nx::network::http::StatusCode::ok;
}
