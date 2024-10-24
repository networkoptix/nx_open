// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_camera_button_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/resource/camera.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::mobile {

using CameraButtonData = core::CameraButtonData;

namespace {

core::OptionalCameraButtonData buttonFromCamera(
    core::SystemContext* context,
    const QnVirtualCameraResourcePtr& currentCamera)
{
    const auto camera = currentCamera.dynamicCast<core::Camera>();
    if (!NX_ASSERT(camera))
        return {};

    if (!camera->isOnline())
        return {};

    if (!context->ptzControllerPool()->controller(camera))
        return {};

    if (camera->getDewarpingParams().enabled)
        return {};

    if (camera->isPtzRedirected())
        return {};

    static const auto kPtzButtonId =
        nx::Uuid::fromArbitraryData(std::string_view("ptz_camera_button_id"));

    return CameraButtonData {
        .id = kPtzButtonId,
        .name = PtzCameraButtonController::tr("Control PTZ"),
        .hint = "",
        .iconName = "qrc:///images/ptz/ptz.png", // Mobile specific, return full path.
        .type = CameraButtonData::Type::instant,
        .enabled = true};
}

} // namespace

struct PtzCameraButtonController::Private
{
    PtzCameraButtonController* const q;
    nx::utils::ScopedConnections connections;

    Private(PtzCameraButtonController* owner):
        q(owner)
    {
    }

    void updateButton()
    {
        const auto oldButton = q->firstButton();
        const auto newButton = q->hasRequiredPermissions()
            ? buttonFromCamera(q->systemContext(), q->camera())
            : core::OptionalCameraButtonData{};

        if (newButton.has_value() == oldButton.has_value())
            return;

        if (newButton)
            q->addOrUpdateButton(*newButton);
        else
            q->removeButton(oldButton->id);
    }

};

PtzCameraButtonController::PtzCameraButtonController(
    CameraButtonData::Group buttonGroup,
    QObject* parent)
    :
    base_type(buttonGroup, Qn::WritePtzPermission, parent),
    d(new Private(this))
{
    connect(this, &BaseCameraButtonController::hasRequiredPermissionsChanged, this,
        [this] { d->updateButton(); });
}

PtzCameraButtonController::~PtzCameraButtonController()
{
}

void PtzCameraButtonController::setResourceInternal(const QnResourcePtr& resource)
{
    d->connections.reset();

    base_type::setResourceInternal(resource);
    if (!resource)
        return;

    if (auto camera = this->camera())
    {
        auto updateButton =
            [this] { d->updateButton(); };

        connect(camera.get(), &QnVirtualCameraResource::statusChanged,
            this, updateButton);
        connect(camera.get(), &QnVirtualCameraResource::mediaDewarpingParamsChanged,
            this, updateButton);
        connect(camera.get(), &QnVirtualCameraResource::propertyChanged,
            this, updateButton);
    }

    d->updateButton();
}

bool PtzCameraButtonController::setButtonActionState(
    const CameraButtonData& /*button*/,
    ActionState /*state*/)
{
    return true;
}

} // namespace nx::vms::client::mobile
