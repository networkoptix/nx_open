#include "external_event_rest_handler.h"

#include <QtCore/QFileInfo>

#include <api/helpers/camera_id_helper.h>
#include <common/common_module.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <media_server/media_server_module.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/server/event/event_connector.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

using namespace nx::network;

namespace nx::vms::server {

ExternalEventRestHandler::ExternalEventRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

rest::Response ExternalEventRestHandler::executeGet(const rest::Request& request)
{
    // TODO: There should be system wide setting to enable this particular method when all GET
    // modifications are disabled by default. This may be required for some 3rd party integrations.
    if (!rest::ini().allowModificationsViaGetMethod)
        return rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

rest::Response ExternalEventRestHandler::executePost(const rest::Request& request)
{
    bool ok = false;
    const auto invalidParameter =
        [&](const QString& name, const QString& value)
        {
            return rest::Response::error(
                http::StatusCode::ok, rest::Result::InvalidParameter, name, value);
        };

    vms::event::EventParameters businessParams;
    if (const auto value = request.param("event_type"))
    {
        businessParams.eventType = QnLexical::deserialized<vms::api::EventType>(*value, {}, &ok);
        if (!ok)
            return invalidParameter("event_type", *value);
    }

    if (const auto value = request.param("timestamp"))
        businessParams.eventTimestampUsec = nx::utils::parseDateTime(*value);

    if (const auto value = request.param("eventResourceId"))
    {
        businessParams.eventResourceId = nx::camera_id_helper::flexibleIdToId(
            request.owner->commonModule()->resourcePool(), *value);
    }

    vms::api::EventState eventState = vms::api::EventState::undefined;
    if (const auto value = request.param("state"))
    {
        eventState = QnLexical::deserialized<vms::api::EventState>(*value, {}, &ok);
        if (!ok)
            return invalidParameter("state", *value);
    }

    if (const auto value = request.param("reasonCode"))
    {
        businessParams.reasonCode = QnLexical::deserialized<vms::api::EventReason>(*value, {}, &ok);
        if (!ok)
            return invalidParameter("reasonCode", *value);
    }

    const auto decoded =
        [&](const QString& name)
        {
            return request.paramOrDefault(name).replace('+', "%20");
        };

    businessParams.resourceName = decoded("inputPortId");
    businessParams.caption = decoded("source");
    businessParams.description = decoded("caption");
    businessParams.inputPortId = decoded("description");

    if (const auto value = request.param("metadata"))
    {
        businessParams.metadata = QJson::deserialized<vms::event::EventMetaData>(
            value->toUtf8(), {}, &ok);
        if (!ok)
            return invalidParameter("metadata", *value);
    }

    if (businessParams.eventTimestampUsec == 0)
        businessParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();

    businessParams.sourceServerId = request.owner->commonModule()->moduleGUID();

    if (businessParams.eventType == vms::api::EventType::undefinedEvent)
        businessParams.eventType = vms::api::EventType::userDefinedEvent;

    const auto& userId = request.owner->accessRights().userId;

    const auto connector = serverModule()->eventConnector();
    QString error;
    if (connector->createEventFromParams(businessParams, eventState, userId, &error))
        return rest::Response::result(rest::JsonResult());

    return rest::Response::error(http::StatusCode::ok, rest::Result::InvalidParameter, error);
}

} // namespace nx::vms::server
