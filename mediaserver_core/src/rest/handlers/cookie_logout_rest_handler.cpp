#include "cookie_logout_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include "nx/network/http/http_types.h"
#include <nx/network/http/custom_headers.h>

int QnCookieLogoutRestHandler::executeGet(const QString &, const QnRequestParams &/*params*/, QnJsonRestResult &/*result*/, const QnRestConnectionProcessor* owner)
{
    QString cookieDeletePattern(lit("%1=deleted; path=/; HttpOnly; expires=Thu, 01 Jan 1970 00:00 : 00 GMT"));

    nx_http::insertHeader(&owner->response()->headers, 
        nx_http::HttpHeader("Set-Cookie", cookieDeletePattern.arg("auth").toUtf8()));
    nx_http::insertHeader(&owner->response()->headers, 
        nx_http::HttpHeader("Set-Cookie", cookieDeletePattern.arg(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)).toUtf8()));

    return nx_http::StatusCode::ok;
}
