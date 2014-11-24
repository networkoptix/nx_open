#include "external_business_event_rest_handler.h"

#include <QtCore/QFileInfo>

#include <core/resource/resource.h>

#include "utils/network/tcp_connection_priv.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "utils/common/synctime.h"
#include <business/business_event_connector.h>

QnExternalBusinessEventRestHandler::QnExternalBusinessEventRestHandler()
{
    QnBusinessEventConnector* connector = qnBusinessRuleConnector;
    connect(this, &QnExternalBusinessEventRestHandler::mserverFailure, connector, &QnBusinessEventConnector::at_mserverFailure);
}

int QnExternalBusinessEventRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    QString eventType;
    QString resourceId;
    QString errStr;
    QnResourcePtr resource;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == QLatin1String("res_id") || params[i].first == QLatin1String("guid"))
            resourceId = params[i].second;
        else if (params[i].first == QLatin1String("event_type"))
            eventType = params[i].second;
    }
    if (resourceId.isEmpty())
        errStr = tr("Parameter 'res_id' is absent or empty. \n");
    else if (eventType.isEmpty())
        errStr = tr("Parameter 'event_type' is absent or empty. \n");
    else {
        resource= qnResPool->getResourceByUniqId(resourceId);
        if (!resource) {
            resource= qnResPool->getResourceById(QnUuid(resourceId));
            if (!resource)
                errStr = tr("Resource with id '%1' not found \n").arg(resourceId);
        }
    }

    if (errStr.isEmpty()) {
        if (eventType == QLatin1String("MServerFailure")) 
        {
            emit mserverFailure(resource,
                 qnSyncTime->currentUSecsSinceEpoch(),
                 QnBusiness::ServerTerminatedReason,
                 QString());
        }
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

int QnExternalBusinessEventRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                                    QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
