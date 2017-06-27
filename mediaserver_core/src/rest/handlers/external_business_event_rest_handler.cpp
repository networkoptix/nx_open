#include "external_business_event_rest_handler.h"

#include <core/resource/resource.h>
#include <core/resource_access/user_access_data.h>

#include "network/tcp_connection_priv.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include "utils/common/synctime.h"
#include <business/business_event_connector.h>
#include <business/business_event_parameters.h>
#include <nx/fusion/model_functions.h>
#include "common/common_module.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/string.h>

QnExternalBusinessEventRestHandler::QnExternalBusinessEventRestHandler()
{
}

int QnExternalBusinessEventRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)

    QnBusinessEventParameters businessParams;
    QString errStr;
    QnBusiness::EventState eventState = QnBusiness::UndefinedState;
    bool ok;

    if (params.contains("event_type")) {
        businessParams.eventType = QnLexical::deserialized<QnBusiness::EventType>(params["event_type"], QnBusiness::EventType(), &ok);
        if (!ok) {
            result.setError(QnRestResult::InvalidParameter, "Invalid value for parameter 'event_type'.");
            return CODE_OK;
        }
    }
    if (params.contains("timestamp"))
        businessParams.eventTimestampUsec = nx::utils::parseDateTime(params["timestamp"]);
    if (params.contains("eventResourceId"))
        businessParams.eventResourceId = QnUuid::fromStringSafe(params["eventResourceId"]);
    if (params.contains("state")) {
        eventState = QnLexical::deserialized<QnBusiness::EventState>(params["state"], QnBusiness::UndefinedState, &ok);
        if (!ok) {
            result.setError(QnRestResult::InvalidParameter, "Invalid value for parameter 'state'.");
            return CODE_OK;
        }
    }
    if (params.contains("reasonCode")) {
        businessParams.reasonCode = QnLexical::deserialized<QnBusiness::EventReason>(params["reasonCode"], QnBusiness::EventReason(), &ok);
        if (!ok) {
            result.setError(QnRestResult::InvalidParameter, "Invalid value for parameter 'reasonCode'.");
            return CODE_OK;
        }
    }
    if (params.contains("inputPortId"))
        businessParams.inputPortId = params["inputPortId"];
    if (params.contains("source"))
        businessParams.resourceName = params["source"];
    if (params.contains("caption"))
        businessParams.caption = params["caption"];
    if (params.contains("description"))
        businessParams.description = params["description"];
    if (params.contains("metadata")) {
        businessParams.metadata = QJson::deserialized<QnEventMetaData>(params["metadata"].toUtf8(), QnEventMetaData(), &ok);
        if (!ok) {
            result.setError(QnRestResult::InvalidParameter, "Invalid value for parameter 'metadata'. It should be a json object. See documentation for more details.");
            return CODE_OK;
        }
    }

    if (businessParams.eventTimestampUsec == 0)
        businessParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();
    businessParams.sourceServerId = owner->commonModule()->moduleGUID();

    if (businessParams.eventType == QnBusiness::UndefinedEvent)
        businessParams.eventType = QnBusiness::UserDefinedEvent; // default value for type is 'CustomEvent'

    const auto& userId = owner->accessRights().userId;

    if (!qnBusinessRuleConnector->createEventFromParams(businessParams, eventState, userId, &errStr))
        result.setError(QnRestResult::InvalidParameter, errStr);

    return CODE_OK;
}
