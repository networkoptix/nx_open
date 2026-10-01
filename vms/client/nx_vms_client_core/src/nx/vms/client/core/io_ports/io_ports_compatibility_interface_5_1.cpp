// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_ports_compatibility_interface_5_1.h"

#include <core/resource/camera_resource.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/device_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/event/action_parameters.h>

namespace {

struct Coordinates
{
    qreal x = 0;
    qreal y = 0;
};
NX_REFLECTION_INSTRUMENT(Coordinates, (x)(y));

} // namespace

namespace nx::vms::client::core {

IoPortsCompatibilityInterface_5_1::IoPortsCompatibilityInterface_5_1(
    SystemContext* systemContext,
    QObject* parent)
    :
    IoPortsCompatibilityInterface(systemContext, parent)
{
}

IoPortsCompatibilityInterface_5_1::~IoPortsCompatibilityInterface_5_1()
{
}

bool IoPortsCompatibilityInterface_5_1::setIoPortState(
    const nx::vms::common::SessionTokenHelperPtr& tokenHelper,
    const QnVirtualCameraResourcePtr& camera,
    const QString& cameraOutputId,
    bool isActive,
    const std::chrono::milliseconds& autoResetTimeout,
    std::optional<nx::vms::api::ResolutionData> targetLockResolutionData,
    ResultCallback callback)
{
    nx::vms::event::ActionParameters actionParameters;
    actionParameters.durationMs = autoResetTimeout.count();
    actionParameters.relayOutputId = cameraOutputId;

    if (targetLockResolutionData)
    {
        const QSize& targetLockResolution = targetLockResolutionData->size;
        Coordinates coords{
            .x = (qreal)targetLockResolution.width(),
            .y = (qreal)targetLockResolution.height()
        };
        actionParameters.text = QString::fromStdString(nx::reflect::json::serialize(coords));
    }

    nx::vms::api::EventActionData actionData;
    actionData.actionType = nx::vms::api::ActionType::cameraOutputAction;
    actionData.toggleState = isActive
        ? nx::vms::api::EventState::active
        : nx::vms::api::EventState::inactive;
    actionData.resourceIds.push_back(camera->getId());
    actionData.params = QJson::serialized(actionParameters);

    auto internalCallback = [this, cameraOutputId, callback](bool success,
                                rest::Handle /*requestId*/,
                                rest::ErrorOrData<QByteArray> response)
    {
        // Servers prior to 6.1 respond to a successful request with an empty body, which
        // can't be parsed as nx::network::rest::Result. Non-empty body may contain an error.
        if (success && response && !response->isEmpty())
        {
            nx::network::rest::Result result;
            if (QJson::deserialize(*response, &result)
                && result.errorId != nx::network::rest::ErrorId::ok)
            {
                response = std::unexpected(std::move(result));
                success = false;
            }
        }

        NX_LOG_RESPONSE(this,
            success,
            response,
            "Extended camera output %1 operation was unsuccessful.",
            cameraOutputId);

        if (callback)
            callback(success);
    };

    if (!systemContext()->connectedServerApi())
        return false;

    return systemContext()->connectedServerApi()->sendRequest<rest::ErrorOrData<QByteArray>>(
        tokenHelper,
        nx::network::http::Method::post,
        "/api/executeEventAction",
        nx::network::rest::Params{},
        QJson::serialized(actionData),
        internalCallback,
        this);
}

} // namespace nx::vms::client::core
