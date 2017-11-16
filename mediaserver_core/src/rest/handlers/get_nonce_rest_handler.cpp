#include "get_nonce_rest_handler.h"

#include <QTimeZone>
#include <QtCore/QJsonDocument>

#include <network/authenticate_helper.h>
#include <network/authutil.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/model/getnonce_reply.h>
#include <nx/network/http/http_client.h>
#include <rest/server/rest_connection_processor.h>

namespace {

bool isResponseOK(const nx_http::HttpClient& client)
{
    if (!client.response())
        return false;
    return client.response()->statusLine.statusCode == nx_http::StatusCode::ok;
}

const int requestTimeoutMs = 10000;

} // namespace

QnGetNonceRestHandler::QnGetNonceRestHandler(const QString& remotePath):
    m_remotePath(remotePath)
{
}

int QnGetNonceRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    if (params.contains("url"))
    {
        if (m_remotePath.isEmpty())
        {
            result.setError(QnRestResult::InvalidParameter, "Parameter url is forbidden");
            return nx_http::StatusCode::forbidden;
        }

        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(requestTimeoutMs);
        client.setSendTimeoutMs(requestTimeoutMs);
        client.setMessageBodyReadTimeoutMs(requestTimeoutMs);

        nx::utils::Url requestUrl(params.value("url"));
        requestUrl.setPath(m_remotePath);

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            result.setError(QnRestResult::CantProcessRequest, "Destination server unreachable");
            return nx_http::StatusCode::ok;
        }

        nx_http::BufferType data;
        while (!client.eof())
            data.append(client.fetchMessageBodyBuffer());

        result = QJson::deserialized<QnJsonRestResult>(data);
        return nx_http::StatusCode::ok;
    }

    QnGetNonceReply reply;
    reply.nonce = QnAuthHelper::instance()->generateNonce();
    reply.realm = nx::network::AppInfo::realm();

    QString userName = params.value("userName");
    if (!userName.isEmpty())
    {
        auto users = owner->resourcePool()->getResources<QnUserResource>().filtered(
            [userName](const QnUserResourcePtr& user)
        {
            return user->getName() == userName;
        });
        if (!users.isEmpty())
            reply.realm = users.front()->getRealm();
    }

    result.setReply(reply);
    return nx_http::StatusCode::ok;
}
