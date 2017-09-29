#include "cookie_login_rest_handler.h"

#include <QTimeZone>

#include <network/authenticate_helper.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/network/http/custom_headers.h>
#include <network/client_authenticate_helper.h>
#include "current_user_rest_handler.h"
#include <api/model/cookie_login_data.h>
#include "cookie_logout_rest_handler.h"
#include <nx/utils/scope_guard.h>

int QnCookieLoginRestHandler::executePost(
    const QString &/*path*/,
    const QnRequestParams &/*params*/,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    bool success = false;
    QnCookieData cookieData = QJson::deserialized<QnCookieData>(body, QnCookieData(), &success);

    auto logoutGuard = makeScopeGuard(
        [owner]
        {
            auto headers = &owner->response()->headers;
            if (headers->find("Set-Cookie") == headers->end())
                QnCookieHelper::addLogoutHeaders(headers);
        });

    if (!success)
    {
        result.setError(QnRestResult::InvalidParameter, "Invalid json object. All fields are mandatory");
        return nx_http::StatusCode::ok;
    }

    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = QnAuthHelper::instance()->authenticateByUrl(
        cookieData.auth,
        QByteArray("GET"),
        *owner->response(),
        &accessRights);
    const_cast<QnRestConnectionProcessor*>(owner)->setAccessRights(accessRights);
    if (authResult == Qn::Auth_CloudConnectError)
    {
        result.setError(QnRestResult::CantProcessRequest, nx::network::AppInfo::cloudName() + " is not accessible yet. Please try again later.");
        return nx_http::StatusCode::ok;
    }
    else if (authResult == Qn::Auth_LDAPConnectError)
    {
        result.setError(QnRestResult::CantProcessRequest, "LDAP server is not accessible yet. Please try again later.");
        return nx_http::StatusCode::ok;
    }
    else if (authResult != Qn::Auth_OK)
    {
        result.setError(QnRestResult::InvalidParameter, "Invalid login or password");
        return nx_http::StatusCode::ok;
    }

    QString cookiePostfix(lit("Path=/; HttpOnly"));
    QString authData = lit("auth=%1;%2")
        .arg(QLatin1String(cookieData.auth))
        .arg(cookiePostfix);
    nx_http::insertHeader(&owner->response()->headers, nx_http::HttpHeader("Set-Cookie", authData.toUtf8()));

    QString clientGuid = lit("%1=%2;")
        .arg(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME))
        .arg(QnUuid::createUuid().toString()) + cookiePostfix;
    nx_http::insertHeader(&owner->response()->headers, nx_http::HttpHeader("Set-Cookie", clientGuid.toUtf8()));

    QnCurrentUserRestHandler currentUser;
    return currentUser.executeGet(QString(), QnRequestParams(), result, owner);
}
