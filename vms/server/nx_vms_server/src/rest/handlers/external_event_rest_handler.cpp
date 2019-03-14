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
#include <nx/vms/server/event/event_connector.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_parameters.h>
#include <media_server/media_server_module.h>
#include <api/helpers/camera_id_helper.h>

using namespace nx;

QnExternalEventRestHandler::QnExternalEventRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
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
    vms::api::EventState eventState = vms::api::EventState::undefined;
    bool ok;

    if (params.contains("event_type"))
    {
        businessParams.eventType = QnLexical::deserialized<vms::api::EventType>(
            params["event_type"], vms::api::EventType(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'event_type'.");
            return nx::network::http::StatusCode::ok;
        }
    }

    if (params.contains("timestamp"))
        businessParams.eventTimestampUsec = nx::utils::parseDateTime(params["timestamp"]);

    if (params.contains("eventResourceId"))
        businessParams.eventResourceId = nx::camera_id_helper::flexibleIdToId(
            owner->commonModule()->resourcePool(),
            params["eventResourceId"]);

    if (params.contains("state"))
    {
        eventState = QnLexical::deserialized<vms::api::EventState>(
            params["state"], vms::api::EventState::undefined, &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'state'.");
            return nx::network::http::StatusCode::ok;
        }
    }

    if (params.contains("reasonCode"))
    {
        businessParams.reasonCode = QnLexical::deserialized<vms::api::EventReason>(
            params["reasonCode"], vms::api::EventReason(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'reasonCode'.");
            return nx::network::http::StatusCode::ok;
        }
    }

    if (params.contains("inputPortId"))
        businessParams.inputPortId = params["inputPortId"];

    auto fromEncoded = [](const QString& s)
    {
        return QUrl::fromPercentEncoding(QString(s).replace('+', "%20").toUtf8());
    };

    if (params.contains("source"))
        businessParams.resourceName = fromEncoded(params["source"]);

    if (params.contains("caption"))
        businessParams.caption = fromEncoded(params["caption"]);

    if (params.contains("description"))
        businessParams.description = fromEncoded(params["description"]);

    if (params.contains("metadata"))
    {
        businessParams.metadata = QJson::deserialized<vms::event::EventMetaData>(
            params["metadata"].toUtf8(), vms::event::EventMetaData(), &ok);
        if (!ok)
        {
            result.setError(QnRestResult::InvalidParameter,
                "Invalid value for parameter 'metadata'. "
                "It should be a json object. See documentation for more details.");
            return nx::network::http::StatusCode::ok;
        }
    }

    if (businessParams.eventTimestampUsec == 0)
        businessParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();

    businessParams.sourceServerId = owner->commonModule()->moduleGUID();

    if (businessParams.eventType == vms::api::EventType::undefinedEvent)
        businessParams.eventType = vms::api::EventType::userDefinedEvent;

    const auto& userId = owner->accessRights().userId;

    if (!serverModule()->eventConnector()->createEventFromParams(
        businessParams, eventState, userId, &errStr))
    {
        result.setError(QnRestResult::InvalidParameter, errStr);
    }

    return nx::network::http::StatusCode::ok;
}
