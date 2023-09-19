// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_camera_button_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::vms::client::core {

struct TwoWayAudioCameraButtonController::Private
{
    TwoWayAudioCameraButtonController * const q;
    const IntercomButtonMode intercomButtonMode;
    const std::unique_ptr<TwoWayAudioController> controller;

    void updateButton();

    bool start(const CameraButton& button);
    bool stop(const CameraButton& button);
    void setButtonChecked(CameraButton button, bool checked);
};

void TwoWayAudioCameraButtonController::Private::updateButton()
{
    const auto oldButton = q->firstButton();
    const auto newButton =
        [this]() -> OptionalCameraButton
        {
            const auto camera = q->camera();
            if (!camera || camera->flags().testFlag(Qn::cross_system))
                return {};

            if (!controller->available() || !q->hasRequiredPermissions())
                return {};

            static const auto kTwoWayAudioButtonId =
                QnUuid::fromArbitraryData(std::string_view("two_way_camera_button_id"));

            using Mode = TwoWayAudioCameraButtonController::IntercomButtonMode;
            if (common::isIntercom(q->camera().staticCast<QnResource>())
                && intercomButtonMode == Mode::checkable)
            {
                // Checkable "Mute" button
                return CameraButton {
                    .id = kTwoWayAudioButtonId,
                    .name = TwoWayAudioCameraButtonController::tr("Unmute"),
                    .checkedName = TwoWayAudioCameraButtonController::tr("Mute"),
                    .hint = "",
                    .iconName = "unmute_call",
                    .checkedIconName = "mute_call",
                    .type = CameraButton::Type::checkable,
                    .enabled = true};
            }

            return CameraButton {
                .id = kTwoWayAudioButtonId,
                .name = TwoWayAudioCameraButtonController::tr("Press and hold to speak"),
                .hint = "",
                .iconName = "mic",
                .type = CameraButton::Type::prolonged,
                .enabled = true};
        }();

    if (newButton.has_value() == oldButton.has_value())
        return;

    if (oldButton && controller->started())
        q->cancelAction(oldButton->id);

    if (newButton)
        q->addOrUpdateButton(*newButton); //< Just add new button, there is no the old one.
    else
        q->removeButton(oldButton->id); //< Remove old button since there is no a new one.
}

bool TwoWayAudioCameraButtonController::Private::start(const CameraButton& button)
{
    const bool result = controller->start(
        [this, button](bool success)
        {
            if (success && button.type == CameraButton::Type::checkable)
                setButtonChecked(button, /*checked*/ true);

            q->safeEmitActionStarted(button.id, success);
        });

    if (!result)
        return false;

    q->addActiveAction(button.id);
    return result;
}

bool TwoWayAudioCameraButtonController::Private::stop(const CameraButton& button)
{
    return controller->stop(
        [this, button](bool success)
        {
            if (!q->actionIsActive(button.id))
                return;

            q->removeActiveAction(button.id);

            if (button.type == CameraButton::Type::checkable)
                setButtonChecked(button, /*checked*/ false);

            q->safeEmitActionStopped(button.id, success);
        });
}

void TwoWayAudioCameraButtonController::Private::setButtonChecked(
    CameraButton button,
    bool checked)
{
    button.checked = checked;
    q->addOrUpdateButton(button);
}

//-------------------------------------------------------------------------------------------------

TwoWayAudioCameraButtonController::TwoWayAudioCameraButtonController(
    IntercomButtonMode intercomButtonMode,
    CameraButton::Group buttonGroup,
    SystemContext* context,
    QObject* parent)
    :
    base_type(buttonGroup, context, Qn::TwoWayAudioPermission, parent),
    d(new Private{.q = this, .intercomButtonMode = intercomButtonMode,
        .controller = std::make_unique<TwoWayAudioController>(context)})
{
    const auto accessController = context->accessController();
    const auto updateControllerSourceId =
        [this, context, accessController]()
    {
        const auto user = accessController->user();
        const auto userId = user ? user->getId() : QnUuid();
        const auto sourceId = DesktopResource::calculateUniqueId(context->peerId(), userId);
        d->controller->setSourceId(sourceId);
    };

    connect(accessController, &AccessController::userChanged, this, updateControllerSourceId);
    updateControllerSourceId();

    connect(d->controller.get(), &TwoWayAudioController::startedChanged, this,
        [this]()
        {
            if (d->controller->started())
                return;

            const auto currentButtons = buttons();
            if (!currentButtons.empty())
                d->setButtonChecked(currentButtons.front(), /*checked*/ false);
        });

    const auto updateButton = [this]() { d->updateButton(); };
    connect(d->controller.get(), &TwoWayAudioController::availabilityChanged, this, updateButton);
    connect(this, &BaseCameraButtonController::hasRequiredPermissionsChanged, this, updateButton);
    connect(this, &BaseCameraButtonController::cameraChanged, d->controller.get(),
        [this, updateButton](const auto& camera, const auto& previousCamera)
        {
            if (previousCamera)
                previousCamera->disconnect(this);

            d->controller->setCamera(camera);

            if (camera)
                connect(camera.get(), &QnResource::flagsChanged, this, updateButton);
        });
}

TwoWayAudioCameraButtonController::~TwoWayAudioCameraButtonController()
{
}

bool TwoWayAudioCameraButtonController::setButtonActionState(
    const CameraButton& button,
    ActionState state)
{
    if (!NX_ASSERT(d->controller->available()))
        return false;

    if (!NX_ASSERT(button.type != CameraButton::Type::instant,
        "Two way audio is not supposed to be instant action"))
    {
        return false;
    }

    return state == ActionState::active
        ? d->start(button)
        : d->stop(button);
}

} // namespace nx::vms::client::core
