#include "cookie_login_rest_handler.h"

#include <QTimeZone>

#include <api/model/cookie_login_data.h>
#include <audit/audit_manager.h>
#include <network/client_authenticate_helper.h>
#include <network/tcp_connection_priv.h>
#include <network/universal_tcp_listener.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/scope_guard.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include <nx/network/app_info.h>
#include "cookie_logout_rest_handler.h"
#include "current_user_rest_handler.h"

int QnCookieLoginRestHandler::executePost(
    const QString &/*path*/,
    const QnRequestParams &/*params*/,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    bool success = false;
    QnCookieData cookieData = QJson::deserialized<QnCookieData>(body, QnCookieData(), &success);
    if (!success)
    {
        result.setError(QnRestResult::InvalidParameter, "Invalid json object. All fields are mandatory");
        return nx::network::http::StatusCode::ok;
    }

    const auto authenticator = QnUniversalTcpListener::authorizer(owner->owner());
    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = authenticator->tryAuthRecord(
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

    if (authResult == Qn::Auth_LDAPConnectError)
    {
        result.setError(QnRestResult::CantProcessRequest,
            "LDAP server is not accessible yet. Please try again later.");
        return nx::network::http::StatusCode::ok;
    }

    if (authResult == Qn::Auth_LockedOut)
    {
        result.setError(QnRestResult::CantProcessRequest,
            "This user on your IP is locked out due to many filed attempts. Please, try again later.");
        return nx::network::http::StatusCode::ok;
    }

    if (authResult != Qn::Auth_OK)
    {
        auto session = owner->authSession();
        session.id = QnUuid::createUuid();
        qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));

        result.setError(QnRestResult::InvalidParameter, "Invalid login or password");
        return nx::network::http::StatusCode::ok;
    }

    authenticator->setAccessCookie(owner->request(), owner->response(), accessRights);
    QnCurrentUserRestHandler currentUser;
    return currentUser.executeGet(QString(), QnRequestParams(), result, owner);
}
