#include "cookie_logout_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include "nx/network/http/http_types.h"
#include <nx/network/http/custom_headers.h>

namespace {

static const QString cookieDeletePattern(lit("%1=deleted; path=/; HttpOnly; expires=Thu, 01 Jan 1970 00:00 : 00 GMT"));

} // namespace

void QnCookieHelper::addLogoutHeaders(nx::network::http::HttpHeaders* outHeaders)
{
    nx::network::http::insertHeader(outHeaders,
        nx::network::http::HttpHeader("Set-Cookie", cookieDeletePattern.arg("auth").toUtf8()));
    nx::network::http::insertHeader(outHeaders,
        nx::network::http::HttpHeader("Set-Cookie", cookieDeletePattern.arg(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)).toUtf8()));
}

int QnCookieLogoutRestHandler::executeGet(const QString &, const QnRequestParams &/*params*/, QnJsonRestResult &/*result*/, const QnRestConnectionProcessor* owner)
{
    QnCookieHelper::addLogoutHeaders(&owner->response()->headers);
    return nx::network::http::StatusCode::ok;
}
