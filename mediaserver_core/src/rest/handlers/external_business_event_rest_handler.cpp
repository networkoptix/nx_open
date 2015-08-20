#include "external_business_event_rest_handler.h"

#include <QtCore/QFileInfo>

#include <core/resource/resource.h>

#include "utils/network/tcp_connection_priv.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "utils/common/synctime.h"
#include <business/business_event_connector.h>
#include <business/business_event_parameters.h>
#include <utils/common/model_functions.h>
#include "common/common_module.h"

QnExternalBusinessEventRestHandler::QnExternalBusinessEventRestHandler()
{
}

int QnExternalBusinessEventRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    
    QnBusinessEventParameters businessParams;
    QString errStr;
    QnBusiness::EventState eventState = QnBusiness::UndefinedState;

    if (params.contains("event_type"))
        businessParams.eventType = QnLexical::deserialized<QnBusiness::EventType>(params["event_type"]);
    if (params.contains("eventTimestamp"))
        businessParams.eventTimestampUsec = parseDateTime(params["eventTimestamp"]);
    if (params.contains("eventResourceId"))
        businessParams.eventResourceId = params["eventResourceId"];
    if (params.contains("state"))
        eventState = QnLexical::deserialized<QnBusiness::EventState>(params["state"]);
    if (params.contains("reasonCode"))
        businessParams.reasonCode = QnLexical::deserialized<QnBusiness::EventReason>(params["reasonCode"]);
    if (params.contains("inputPortId"))
        businessParams.inputPortId = params["inputPortId"];
    if (params.contains("deviceName"))
        businessParams.resourceName = params["deviceName"];
    if (params.contains("caption"))
        businessParams.caption = params["caption"];
    if (params.contains("description"))
        businessParams.description = params["description"];
    
    if (businessParams.eventTimestampUsec == 0)
        businessParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    businessParams.sourceServerId = qnCommon->moduleGUID();

    // default value for type is 'CustomEvent'
    if (businessParams.eventType == QnBusiness::UndefinedEvent) {
        businessParams.eventType = eventState == QnBusiness::UndefinedState ? QnBusiness::CustomInstantEvent : QnBusiness::CustomProlongedEvent;
    }

    if (!qnBusinessRuleConnector->createEventFromParams(businessParams, eventState))
        result.setError(QnRestResult::InvalidParameter, "Invalid event parameters");

    return CODE_OK;
}
