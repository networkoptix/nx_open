#include "cookie_logout_rest_handler.h"

#include <rest/server/rest_connection_processor.h>
#include "nx/network/http/http_types.h"
#include <nx/network/http/custom_headers.h>
#include <network/authenticate_helper.h>

static const std::map<QByteArray, QByteArray> kCookies =
{
    { Qn::URL_QUERY_AUTH_KEY_NAME, "HttpOnly" },
    { Qn::EC2_RUNTIME_GUID_HEADER_NAME, "HttpOnly" },
    { Qn::CSRF_TOKEN_COOKIE_NAME, "" },
};

void QnCookieHelper::addLogoutHeaders(nx::network::http::HttpHeaders* outHeaders)
{
    // TODO: Remove UUID and CSRF tocken for verification failure in
    // QnAuthHelper::doCookieAuthorization.
    for (const auto& cookie: kCookies)
    {
        QByteArray data = cookie.first;
        data += "=deleted; Path=/";
        if (!cookie.second.isEmpty())
            data += "; " + cookie.second;

        data += "; expires=Thu, 01 Jan 1970 00:00 : 00 GMT";
        nx::network::http::insertHeader(outHeaders, nx::network::http::HttpHeader("Set-Cookie", data));
    }
}

int QnCookieLogoutRestHandler::executeGet(
    const QString &, const QnRequestParams &/*params*/,
    QnJsonRestResult &/*result*/, const QnRestConnectionProcessor* owner)
{
    QnAuthHelper::instance()->removeCsrfToken(nx::network::http::getHeaderValue(
        owner->request().headers, Qn::CSRF_TOKEN_COOKIE_NAME));

    QnCookieHelper::addLogoutHeaders(&owner->response()->headers);
    return nx::network::http::StatusCode::ok;
}
