#include "get_nonce_rest_handler.h"

#include <QTimeZone>

#include <network/authenticate_helper.h>
#include <network/authutil.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

int QnGetNonceRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    NonceReply reply;
    reply.nonce = QnAuthHelper::instance()->generateNonce();
    reply.realm = QnAppInfo::realm();

    QString userName = params.value("userName");
    if (!userName.isEmpty())
    {
        auto users = qnResPool->getResources<QnUserResource>().filtered(
            [userName](const QnUserResourcePtr& user)
        {
            return user->getName() == userName;
        });
        if (!users.isEmpty())
            reply.realm = users.front()->getRealm();
    }

    result.setReply(reply);
    return CODE_OK;
}
