// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_tracking_button_controller.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <ui/graphics/items/controls/object_tracking_button.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kTriggerButtonHeight = 40;

const QString kAutoTrackingPortName =
    QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::autoTracking));

bool cameraHasObjectTracking(const QnVirtualCameraResourcePtr& camera)
{
    NX_ASSERT(camera);

    const auto portDescriptions = camera->ioPortDescriptions();
    return std::any_of(portDescriptions.begin(), portDescriptions.end(),
        [](const auto& portData)
        {
            return portData.outputName == kAutoTrackingPortName;
        });
}

} // namespace

ObjectTrackingButtonController::ObjectTrackingButtonController(
    QnMediaResourceWidget* mediaResourceWidget)
    :
    base_type(mediaResourceWidget)
{
    if (m_camera.isNull())
        return;

    if (cameraHasObjectTracking(m_camera))
        openIoModuleConnection();

    connect(m_camera.get(), &QnVirtualCameraResource::propertyChanged,
        this, [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
            if (key == ResourcePropertyKey::kIoSettings && cameraHasObjectTracking(m_camera))
                openIoModuleConnection();
        });
}

ObjectTrackingButtonController::~ObjectTrackingButtonController()
{
}

void ObjectTrackingButtonController::createButtons()
{
    if (m_camera.isNull())
        return;

    if (m_objectTrackingIsActive)
    {
        ensureButtonState(ExtendedCameraOutput::autoTracking,
            [this]
            {
                auto objectTrackingButton = new ObjectTrackingButton(m_parentWidget);

                connect(this, &ObjectTrackingButtonController::ioStateChanged,
                    [this](const QnIOStateData& value)
                    {
                        if (value.id  == getOutputId(ExtendedCameraOutput::autoTracking)
                            && !value.isActive)
                        {
                            removeButtons();
                        }
                    });

                connect(objectTrackingButton, &ObjectTrackingButton::clicked,
                    [this, objectTrackingButton]
                    {
                        buttonClickHandler(
                            objectTrackingButton,
                            ExtendedCameraOutput::autoTracking,
                            nx::vms::api::EventState::inactive);
                    });

                return objectTrackingButton;
            });
    }
    else
    {
        removeButton(ExtendedCameraOutput::autoTracking);
    }
}

void ObjectTrackingButtonController::removeButtons()
{
    ButtonController::removeButtons();
}

void ObjectTrackingButtonController::handleChangedIOState(const QnIOStateData& value)
{
    if (value.id == getOutputId(ExtendedCameraOutput::autoTracking))
    {
        m_objectTrackingIsActive = value.isActive;
        emit requestButtonCreation();
    }

    emit ioStateChanged(value);
}

void ObjectTrackingButtonController::handleApiReply(
    GraphicsWidget* /*button*/,
    bool /*success*/,
    rest::Handle /*requestId*/,
    nx::network::rest::JsonResult& /*result*/)
{
}

} // namespace nx::vms::client::desktop
