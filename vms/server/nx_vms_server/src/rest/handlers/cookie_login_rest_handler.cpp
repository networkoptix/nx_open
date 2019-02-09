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

#include "cookie_logout_rest_handler.h"
#include "current_user_rest_handler.h"
#include <common/common_module.h>

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

    const auto authenticator = QnUniversalTcpListener::authenticator(owner->owner());
    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = authenticator->tryAuthRecord(
        owner->getForeignAddress().address,
        cookieData.auth,
        QByteArray("GET"),
        *owner->response(),
        &accessRights);

    const_cast<QnRestConnectionProcessor*>(owner)->setAccessRights(accessRights);
    if (authResult == Qn::Auth_CloudConnectError
        || authResult == Qn::Auth_LDAPConnectError
        || authResult == Qn::Auth_LockedOut)
    {
        result.setError(QnRestResult::CantProcessRequest, Qn::toErrorMessage(authResult));
        return nx::network::http::StatusCode::ok;
    }

    if (authResult != Qn::Auth_OK)
    {
        auto session = owner->authSession();
        session.id = QnUuid::createUuid();
        auto auditManager = owner->commonModule()->auditManager();
        auditManager->addAuditRecord(auditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));

        result.setError(QnRestResult::CantProcessRequest, Qn::toErrorMessage(authResult));
        return nx::network::http::StatusCode::ok;
    }

    authenticator->setAccessCookie(
        owner->request(), owner->response(), accessRights, owner->isConnectionSecure());

    QnCurrentUserRestHandler currentUser;
    return currentUser.executeGet(QString(), QnRequestParams(), result, owner);
}
