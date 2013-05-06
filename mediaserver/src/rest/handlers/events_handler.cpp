#include "events_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"
#include "business/actions/abstract_business_action.h"
#include "events/events_db.h"

QnRestEventsHandler::QnRestEventsHandler()
{

}

int QnRestEventsHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    QnTimePeriod period;
    period.durationMs = -1;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "from")
            period.startTimeMs = params[i].second.toLongLong();
    }

    if (period.startTimeMs == 0)
    {
        result.append("<root>\n");
        result.append("Parameter 'from' MUST be specified");
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "to")
            period.durationMs = params[i].second.toLongLong() - period.startTimeMs;
    }

    QList<QnAbstractBusinessActionPtr> actions = qnEventsDB->getActions(period);

    result.append(QString("<pong>%1</pong>\n").arg(serverGuid()).toUtf8());
    return CODE_OK;
}

int QnRestEventsHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnRestEventsHandler::description() const
{
    return 
        "Returns events log"
        "<BR>Param <b>from</b> - start of time period at ms sicnec 1.1.1970 (UTC format)"
        "<BR>Param <b>to</b> - end of time period at ms sicnec 1.1.1970 (UTC format). Optional"
        "<BR>Param <b>format</b> - allowed values: <b>text</b>, <b>protobuf</b>";
}
