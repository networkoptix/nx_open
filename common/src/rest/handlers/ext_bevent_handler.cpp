#include "ext_bevent_handler.h"

#include <QtCore/QFileInfo>

#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resource_managment/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "utils/common/synctime.h"
#include <business/business_event_connector.h>
#include <business/events/reasoned_business_event.h>

QnExternalBusinessEventHandler::QnExternalBusinessEventHandler()
{

}

int QnExternalBusinessEventHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    QString eventType;
    QString resourceId;
    QString errStr;
    QnResourcePtr resource;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first.toLocal8Bit() == "res_id" || params[i].first.toLocal8Bit() == "guid")
            resourceId = params[i].second;
        else if (params[i].first.toLocal8Bit() == "event_type")
            eventType = params[i].second;
    }
    if (resourceId.isEmpty())
        errStr = tr("Parameter 'res_id' is absent or empty. \n");
    else if (eventType.isEmpty())
        errStr = tr("Parameter 'event_type' is absent or empty. \n");
    else {
        resource= qnResPool->getResourceByUniqId(resourceId);
        if (!resource) {
            resource= qnResPool->getResourceByGuid(resourceId);
            if (!resource)
                errStr = tr("Resource with id '%1' not found \n").arg(resourceId);
        }
    }

    if (errStr.isEmpty()) {
        if (eventType == QLatin1String("MServerFailure"))
            qnBusinessRuleConnector->at_mserverFailure(resource,
                                                       qnSyncTime->currentUSecsSinceEpoch(),
                                                       QnBusiness::MServerIssueTerminated);
        //else if (eventType == "UserEvent")
        //    bEvent = new QnUserDefinedBusinessEvent(); // todo: not implemented
        else if (errStr.isEmpty())
            errStr = QString(QLatin1String("Unknown business event type '%1' \n")).arg(eventType);
    }

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    result.append("<root>\n");
    result.append("OK\n");
    result.append("</root>\n");

    return CODE_OK;
}

int QnExternalBusinessEventHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnExternalBusinessEventHandler::description(TCPSocket *) const
{
    return QLatin1String(
        "Process external business event\n"
        "<BR>Param <b>event_type</b> - eventType. supported values: 'MServerFailure', 'UserEvent'\n"
        "<BR>Param <b>res_id</b> - resource (media server or camera) uniq id\n"
        "<BR><b>Return</b> XML with error string or 'OK' message.\n"
    );
}
