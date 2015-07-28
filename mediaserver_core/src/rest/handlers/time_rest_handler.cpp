#include "time_rest_handler.h"

#include <QTimeZone>

#include <api/model/time_reply.h>

#include <network/authenticate_helper.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>


int QnTimeRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QnTimeReply reply;
    reply.timeZoneOffset = currentTimeZone() * 1000ll;
    if (params.contains("local"))
        reply.utcTime =  QDateTime::currentDateTime().toMSecsSinceEpoch();
    else
        reply.utcTime = qnSyncTime->currentMSecsSinceEpoch();
    reply.timezoneId = QDateTime::currentDateTime().timeZone().id();
    reply.realm = QnAuthHelper::REALM;

    result.setReply(reply);
    return CODE_OK;
}
