// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_camera_button_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/access/access_controller.h>

namespace nx::vms::client::core {

namespace {

const auto lowerBound =
    [](auto& buttons, const QnUuid& id)
    {
        return std::lower_bound(buttons.begin(), buttons.end(), id,
            [](const CameraButton& left, const QnUuid& id)
            {
                return left.id < id;
            });
    };

} // namespace

struct BaseCameraButtonController::Private
{
    BaseCameraButtonController* const q;
    const CameraButton::Group buttonGroup;

    CameraButtons buttons;
    QnVirtualCameraResourcePtr camera;
    QnUuidSet activeActions;

    AccessController::NotifierPtr accessNotifier;
    Qn::Permissions requiredPermissions = Qn::NoPermissions;
    bool hasRequiredPermissions = false;

    void setupCamera();

    void cancelActiveActions();

    void handleCameraChanged();
};

void BaseCameraButtonController::Private::setupCamera()
{
    const auto resourceId = q->resourceId();
    const auto newCamera = resourceId.isNull()
        ? QnVirtualCameraResourcePtr()
        : q->systemContext()->resourcePool()->getResourceById<QnVirtualCameraResource>(resourceId);

    if (newCamera == camera)
        return;

    cancelActiveActions();

    if (camera)
        camera.reset();

    const auto previousCamera = camera;
    camera = newCamera;
    emit q->cameraChanged(camera, previousCamera);
}

void BaseCameraButtonController::Private::cancelActiveActions()
{
    while (!activeActions.empty())
        q->cancelAction(*activeActions.cbegin());
}

void BaseCameraButtonController::Private::handleCameraChanged()
{
    const auto camera = q->camera();
    const auto accessController = q->systemContext()->accessController();

    accessNotifier = camera
        ? accessController->createNotifier(camera)
        : AccessController::NotifierPtr{};

    const auto updatePermissions =
        [this](const QnResourcePtr& camera, Qn::Permissions current, Qn::Permissions /*old*/)
        {
            const bool value = camera && (current & requiredPermissions == requiredPermissions);
            if (hasRequiredPermissions == value)
                return;

            hasRequiredPermissions = value;
            emit q->hasRequiredPermissionsChanged();
        };

    if (accessNotifier)
    {
        QObject::connect(accessNotifier.get(), &AccessController::Notifier::permissionsChanged,
            q, updatePermissions);
    }

    const auto currentPermissions = camera
        ? accessController->permissions(camera)
        : Qn::Permissions{};
    updatePermissions(camera, currentPermissions, /*oldPermissions*/ {});
}

//-------------------------------------------------------------------------------------------------

BaseCameraButtonController::BaseCameraButtonController(
    CameraButton::Group buttonGroup,
    SystemContext* context,
    Qn::Permissions requiredPermissions,
    QObject* parent)
    :
    base_type(context, parent),
    d(new Private{.q = this, .buttonGroup = buttonGroup,
        .requiredPermissions = requiredPermissions })
{
    connect(this, &AbstractCameraButtonController::resourceIdChanged, this,
        [this]() { d->setupCamera(); });

    connect(this, &AbstractCameraButtonController::buttonChanged, this,
        [this](const CameraButton& button, CameraButton::Fields fields)
        {
            // Try to cancel action.
            if (fields.testFlag(CameraButton::Field::type))
                cancelAction(button.id);
        });

    connect(this, &BaseCameraButtonController::cameraChanged,
        this, [this]() { d->handleCameraChanged(); });
}

BaseCameraButtonController::~BaseCameraButtonController()
{
    d->cancelActiveActions();
}

CameraButtons BaseCameraButtonController::buttons() const
{
    return d->buttons;
}

OptionalCameraButton BaseCameraButtonController::button(const QnUuid& buttonId) const
{
    const auto it = lowerBound(d->buttons, buttonId);
    return it != d->buttons.end() && it->id == buttonId
        ? OptionalCameraButton(*it)
        : OptionalCameraButton();
}

bool BaseCameraButtonController::startAction(const QnUuid& buttonId)
{
    const auto& targetButton = button(buttonId);
    if (!NX_ASSERT(targetButton, "Can't find button to activate!"))
        return false;

    if (!NX_ASSERT(!actionIsActive(buttonId), "Can't activate button twice!"))
        return false;

    return setButtonActionState(*targetButton, ActionState::active);
}

bool BaseCameraButtonController::stopAction(const QnUuid& buttonId)
{
    const auto& targetButton = button(buttonId);
    if (!NX_ASSERT(targetButton, "Can't find a button to activate an action!"))
        return false;

    if (!actionIsActive(buttonId))
        return false;

    return setButtonActionState(*targetButton, ActionState::inactive);
}

bool BaseCameraButtonController::cancelAction(const QnUuid& buttonId)
{
    const auto& targetButton = button(buttonId);
    if (!NX_ASSERT(targetButton, "Can't find a button to activate an action!"))
        return false;

    if (!actionIsActive(buttonId))
        return false;

    stopAction(buttonId);
    d->activeActions.remove(buttonId);
    safeEmitActionCancelled(buttonId);
    return true;
}

bool BaseCameraButtonController::actionIsActive(const QnUuid& buttonId) const
{
    return d->activeActions.contains(buttonId);
}

bool BaseCameraButtonController::hasRequiredPermissions() const
{
    return d->hasRequiredPermissions;
}

QnVirtualCameraResourcePtr BaseCameraButtonController::camera() const
{
    return d->camera;
}

bool BaseCameraButtonController::addOrUpdateButton(const CameraButton& sourceButton)
{
    auto button = sourceButton;
    button.group = d->buttonGroup;

    auto it = lowerBound(d->buttons, button.id);
    const bool buttonIsMissing = it == d->buttons.end() || it->id != button.id;
    if (buttonIsMissing)
    {
        it = d->buttons.insert(it, button);
        emit buttonAdded(button);
    }

    auto& currentData = *it;
    if (const auto diff = button.difference(currentData))
    {
        currentData = button;
        emit buttonChanged(button, diff);
    }

    return buttonIsMissing;
}

bool BaseCameraButtonController::removeButton(const QnUuid& buttonId)
{
    const auto it = lowerBound(d->buttons, buttonId);
    if (it == d->buttons.end() || it->id != buttonId)
        return false;

    if (actionIsActive(buttonId))
        cancelAction(buttonId);

    emit buttonRemoved(buttonId);
    return true;
}

OptionalCameraButton BaseCameraButtonController::firstButton() const
{
    const auto& currentButtons = buttons();
    return currentButtons.empty()
        ? OptionalCameraButton{}
        : OptionalCameraButton(currentButtons.front());
}

void BaseCameraButtonController::addActiveAction(const QnUuid& buttonId)
{
    if (NX_ASSERT(!actionIsActive(buttonId), "Can't activate button twice!"))
        d->activeActions.insert(buttonId);
}

void BaseCameraButtonController::removeActiveAction(const QnUuid& buttonId)
{
    NX_ASSERT(d->activeActions.remove(buttonId), "Can't find active action");
}

bool BaseCameraButtonController::setButtonActionState(
    const CameraButton& /*button*/,
    ActionState /*state*/)
{
    return false;
}

} // namespace nx::vms::client::core
