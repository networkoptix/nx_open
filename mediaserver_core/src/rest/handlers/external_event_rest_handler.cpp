#include "external_event_rest_handler.h"

#include <QtCore/QFileInfo>

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>
#include <nx/fusion/model_functions.h>
#include <nx/mediaserver/event/event_connector.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_parameters.h>

using namespace nx;

QnExternalEventRestHandler::QnExternalEventRestHandler()
{
}

int QnExternalEventRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    vms::event::EventParameters businessParams;
    QString errStr;
    vms::event::EventState eventState = vms::event::UndefinedState;
    bool ok;

    if (params.contains("event_type"))
    {
        businessParams.eventType = QnLexical::deserialized<vms::event::EventType>(
            params["event_type"], vms::event::EventType(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'event_type'.");
            return CODE_OK;
        }
    }

    if (params.contains("timestamp"))
        businessParams.eventTimestampUsec = nx::utils::parseDateTime(params["timestamp"]);

    if (params.contains("eventResourceId"))
        businessParams.eventResourceId = QnUuid::fromStringSafe(params["eventResourceId"]);

    if (params.contains("state"))
    {
        eventState = QnLexical::deserialized<vms::event::EventState>(
            params["state"], vms::event::UndefinedState, &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'state'.");
            return CODE_OK;
        }
    }

    if (params.contains("reasonCode"))
    {
        businessParams.reasonCode = QnLexical::deserialized<vms::event::EventReason>(
            params["reasonCode"], vms::event::EventReason(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'reasonCode'.");
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

    if (params.contains("metadata"))
    {
        businessParams.metadata = QJson::deserialized<vms::event::EventMetaData>(
            params["metadata"].toUtf8(), vms::event::EventMetaData(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'metadata'. "
                "It should be a json object. See documentation for more details.");
            return CODE_OK;
        }
    }

    if (businessParams.eventTimestampUsec == 0)
        businessParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();

    businessParams.sourceServerId = owner->commonModule()->moduleGUID();

    if (businessParams.eventType == vms::event::UndefinedEvent)
        businessParams.eventType = vms::event::UserDefinedEvent; // default value for type is 'CustomEvent'

    const auto& userId = owner->accessRights().userId;

    if (!qnEventRuleConnector->createEventFromParams(businessParams, eventState, userId, &errStr))
        result.setError(QnRestResult::InvalidParameter, errStr);

    return CODE_OK;
}
