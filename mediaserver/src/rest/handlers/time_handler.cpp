#include "time_handler.h"

#include <api/model/time_reply.h>

#include <utils/network/tcp_connection_priv.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

QnTimeHandler::QnTimeHandler() {}

int QnTimeHandler::executeGet(const QString &, const QnRequestParams &, QnJsonRestResult &result) {
    QnTimeReply reply;
    reply.timeZoneOffset = currentTimeZone() * 1000ll;
    reply.utcTime = qnSyncTime->currentMSecsSinceEpoch();

    result.setReply(reply);
    return CODE_OK;
}

QString QnTimeHandler::description() const {
    return "Returns server UTC time and time zone";
}
