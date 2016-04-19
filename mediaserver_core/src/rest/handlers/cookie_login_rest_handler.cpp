#include "cookie_login_rest_handler.h"

#include <QTimeZone>

#include <network/authenticate_helper.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <rest/server/rest_connection_processor.h>
#include <http/custom_headers.h>
#include <network/client_authenticate_helper.h>
#include "current_user_rest_handler.h"

struct QnCookieData
{
    QnLatin1Array auth;
};
#define QnCookieData_Fields (auth)

QN_FUSION_DECLARE_FUNCTIONS(QnCookieData, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnCookieData), (json), _Fields)

int QnCookieLoginRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    bool success = false;
    QnCookieData cookieData = QJson::deserialized<QnCookieData>(body, QnCookieData(), &success);
    if (!success)
    {
        result.setError(QnRestResult::InvalidParameter, "Invalid json object. All fields are mandatory");
        return nx_http::StatusCode::ok;
    }

    QnUuid authUserId;
    Qn::AuthResult authResult = QnAuthHelper::instance()->authenticateByUrl(
        cookieData.auth,
        QByteArray("GET"), 
        *owner->response(), 
        &authUserId);
    const_cast<QnRestConnectionProcessor*>(owner)->setAuthUserId(authUserId);
    if (authResult != Qn::Auth_OK)
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
