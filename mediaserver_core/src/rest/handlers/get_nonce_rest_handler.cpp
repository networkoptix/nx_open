#include "get_nonce_rest_handler.h"

#include <QTimeZone>

#include <network/authenticate_helper.h>
#include <network/tcp_connection_priv.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>


struct QnNoncemReply
{
    QString nonce;
    QString realm;
};
#define QnNoncemReply_Fields (nonce)(realm)

QN_FUSION_DECLARE_FUNCTIONS(QnNoncemReply, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnNoncemReply), (json), _Fields, (optional, true))

int QnGetNonceRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    QnNoncemReply reply;
    reply.nonce = QnAuthHelper::instance()->generateNonce();
    reply.realm = QnAppInfo::realm();
    result.setReply(reply);
    return CODE_OK;
}
