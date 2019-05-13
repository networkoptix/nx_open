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

JsonRestResponse QnCookieLoginRestHandler::executePost(
    const JsonRestRequest& request, const QByteArray& body)
{
    JsonRestResponse response(nx::network::http::StatusCode::ok);

    bool success = false;
    QnCookieData cookieData = QJson::deserialized<QnCookieData>(body, QnCookieData(), &success);
    if (!success)
    {
        response.json.setError(
            QnRestResult::InvalidParameter, "Invalid json object. All fields are mandatory");
        return response;
    }

    const auto authenticator = QnUniversalTcpListener::authenticator(request.owner->owner());
    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = authenticator->tryAuthRecord(
        request.owner->getForeignAddress().address,
        cookieData.auth,
        QByteArray("GET"),
        *request.owner->response(),
        &accessRights);

    nx::network::http::insertOrReplaceHeader(
        &response.httpHeaders,
        {Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(authResult).toUtf8()});

    const_cast<QnRestConnectionProcessor*>(request.owner)->setAccessRights(accessRights);
    if (authResult == Qn::Auth_CloudConnectError
        || authResult == Qn::Auth_LDAPConnectError
        || authResult == Qn::Auth_LockedOut)
    {
        response.json.setError(QnRestResult::CantProcessRequest, Qn::toErrorMessage(authResult));
        return response;
    }

    if (authResult != Qn::Auth_OK)
    {
        auto session = request.owner->authSession();
        session.id = QnUuid::createUuid();
        auto auditManager = request.owner->commonModule()->auditManager();
        auditManager->addAuditRecord(auditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));

        NX_DEBUG(this, "Failure to authenticate request %1 with error %2. userName: %3",
            request.path,
            Qn::toErrorMessage(authResult),
            session.userName);


        response.json.setError(QnRestResult::CantProcessRequest, Qn::toErrorMessage(authResult));
        return response;
    }

    authenticator->setAccessCookie(
        request.owner->request(), request.owner->response(),
        accessRights, request.owner->isConnectionSecure());

    QnCurrentUserRestHandler currentUser;
    response.statusCode = static_cast<nx::network::http::StatusCode::Value>(currentUser.executeGet(
        QString(), QnRequestParams(), response.json, request.owner));

    return response;
}
