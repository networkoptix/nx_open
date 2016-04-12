#include "cookie_logout_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include "nx/network/http/httptypes.h"

int QnCookieLogoutRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    nx_http::insertHeader(&owner->response()->headers, nx_http::HttpHeader("Set-Cookie", QByteArray()));
    return nx_http::StatusCode::ok;
}
