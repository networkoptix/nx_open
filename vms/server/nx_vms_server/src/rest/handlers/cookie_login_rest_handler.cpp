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

using namespace nx::network;

static void addResultHeader(rest::Response* response, Qn::AuthResult code)
{
    http::insertOrReplaceHeader(
        &response->httpHeaders,
        {Qn::AUTH_RESULT_HEADER_NAME, QnLexical::serialized(code).toUtf8()});
}

static rest::Response errorResponse(Qn::AuthResult code)
{
    auto response = rest::Response::error(
        http::StatusCode::ok, QnRestResult::CantProcessRequest, Qn::toErrorMessage(code));

    addResultHeader(&response, code);
    return response;
}

rest::Response QnCookieLoginRestHandler::executePost(const rest::Request& request)
{
    const auto cookieData = request.parseContentOrThrow<QnCookieData>();

    const auto authenticator = QnUniversalTcpListener::authenticator(request.owner->owner());
    Qn::UserAccessData accessRights;
    Qn::AuthResult authResult = authenticator->tryAuthRecord(
        request.owner->getForeignAddress().address,
        cookieData.auth,
        QByteArray("GET"),
        *request.owner->response(),
        &accessRights);

    const_cast<QnRestConnectionProcessor*>(request.owner)->setAccessRights(accessRights);
    if (authResult == Qn::Auth_CloudConnectError
        || authResult == Qn::Auth_LDAPConnectError
        || authResult == Qn::Auth_LockedOut)
    {
        return errorResponse(authResult);
    }

    if (authResult != Qn::Auth_OK)
    {
        auto session = request.owner->authSession();
        session.id = QnUuid::createUuid();
        auto auditManager = request.owner->commonModule()->auditManager();
        auditManager->addAuditRecord(auditManager->prepareRecord(session, Qn::AR_UnauthorizedLogin));
        return errorResponse(authResult);
    }

    authenticator->setAccessCookie(
        request.owner->request(), request.owner->response(),
        accessRights, request.owner->isConnectionSecure());

    auto response = QnCurrentUserRestHandler().executeGet(request);
    addResultHeader(&response, authResult);
    return response;
}
