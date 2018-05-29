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
#include <audit/audit_manager.h>

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
        return nx::network::http::StatusCode::ok;
    }

    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = QnAuthHelper::instance()->authenticateByUrl(
        owner->socket()->getForeignAddress().address,
        cookieData.auth,
        QByteArray("GET"),
        *owner->response(),
        &accessRights);

    const_cast<QnRestConnectionProcessor*>(owner)->setAccessRights(accessRights);
    if (authResult == Qn::Auth_CloudConnectError)
    {
        result.setError(QnRestResult::CantProcessRequest,
            nx::network::AppInfo::cloudName() + " is not accessible yet. Please try again later.");
        return nx::network::http::StatusCode::ok;
    }
    else if (authResult == Qn::Auth_LDAPConnectError)
    {
        result.setError(QnRestResult::CantProcessRequest,
            "LDAP server is not accessible yet. Please try again later.");
        return nx::network::http::StatusCode::ok;
    }
    else if (authResult != Qn::Auth_OK)
    {
        auto session = owner->authSession();
        session.id = QnUuid::createUuid();
        qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));

        result.setError(QnRestResult::InvalidParameter, "Invalid login or password");
        return nx::network::http::StatusCode::ok;
    }

    const auto setCookie =
        [&](const QByteArray& name, const QByteArray& value, const QByteArray& options)
    {
        QByteArray data = name;
        data += "=";
        data += value;
        data += "; Path=/";
        if (!options.isEmpty())
            data += "; " + options;
        nx::network::http::insertHeader(
            &owner->response()->headers,
            nx::network::http::HttpHeader("Set-Cookie", data));
    };

    setCookie(Qn::URL_QUERY_AUTH_KEY_NAME, cookieData.auth, "HttpOnly");
    setCookie(Qn::EC2_RUNTIME_GUID_HEADER_NAME, QnUuid::createUuid().toByteArray(), "HttpOnly");
    setCookie(Qn::CSRF_TOKEN_COOKIE_NAME, QnAuthHelper::instance()->newCsrfToken(), "");

    QnCurrentUserRestHandler currentUser;
    return currentUser.executeGet(QString(), QnRequestParams(), result, owner);
}
