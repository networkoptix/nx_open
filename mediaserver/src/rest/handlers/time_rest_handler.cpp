#include "time_rest_handler.h"

#include <api/model/time_reply.h>

#include <utils/network/tcp_connection_priv.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <QTimeZone>

int QnTimeRestHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QnTimeReply reply;
    reply.timeZoneOffset = currentTimeZone() * 1000ll;
    reply.utcTime = qnSyncTime->currentMSecsSinceEpoch();
    reply.timezoneId = QDateTime::currentDateTime().timeZone().id();

    result.setReply(reply);
    return CODE_OK;
}
