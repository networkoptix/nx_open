#include "get_nonce_rest_handler.h"

#include <QTimeZone>
#include <QtCore/QJsonDocument>

#include <api/model/getnonce_reply.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <network/authutil.h>
#include <network/tcp_connection_priv.h>
#include <network/universal_tcp_listener.h>
#include <nx/network/http/http_client.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

namespace {

bool isResponseOK(const nx::network::http::HttpClient& client)
{
    if (!client.response())
        return false;
    return client.response()->statusLine.statusCode == nx::network::http::StatusCode::ok;
}

const std::chrono::seconds requestTimeout(10);

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
            return nx::network::http::StatusCode::forbidden;
        }

        nx::network::http::HttpClient client;
        client.setResponseReadTimeout(requestTimeout);
        client.setSendTimeout(requestTimeout);
        client.setMessageBodyReadTimeout(requestTimeout);

        nx::utils::Url requestUrl(params.value("url"));
        requestUrl.setPath(m_remotePath);

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            result.setError(QnRestResult::CantProcessRequest, "Destination server unreachable");
            return nx::network::http::StatusCode::ok;
        }

        nx::network::http::BufferType data;
        while (!client.eof())
            data.append(client.fetchMessageBodyBuffer());

        result = QJson::deserialized<QnJsonRestResult>(data);
        return nx::network::http::StatusCode::ok;
    }

    QnGetNonceReply reply;
    reply.nonce = QnUniversalTcpListener::authenticator(owner->owner())->generateNonce();
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
    return nx::network::http::StatusCode::ok;
}
