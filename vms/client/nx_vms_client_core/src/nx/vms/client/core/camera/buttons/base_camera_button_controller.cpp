// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_camera_button_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

namespace {

const auto lowerBound =
    [](auto& buttons, const nx::Uuid& id)
    {
        return std::lower_bound(buttons.begin(), buttons.end(), id,
            [](const CameraButton& left, const nx::Uuid& id)
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
    UuidSet activeActions;

    AccessController::NotifierPtr accessNotifier;
    Qn::Permissions requiredPermissions = Qn::NoPermissions;
    bool hasRequiredPermissions = false;

    void setupCamera();

    void cancelActiveActions();
};

void BaseCameraButtonController::Private::setupCamera()
{
    const auto newCamera = q->resource().dynamicCast<QnVirtualCameraResource>();

    if (newCamera == camera)
        return;

    cancelActiveActions();

    if (camera)
        camera.reset();

    const bool hadRequiredPermissions = hasRequiredPermissions;

    camera = newCamera;

    AccessController* accessController = camera
        ? q->systemContext()->accessController()
        : nullptr;

    accessNotifier = accessController
        ? accessController->createNotifier(camera)
        : AccessController::NotifierPtr{};

    if (accessNotifier)
    {
        const auto updateHasRequiredPermissions =
            [this](const QnResourcePtr& camera, Qn::Permissions permissions)
            {
                if (!NX_ASSERT(camera == this->camera))
                    return;

                const bool newHasRequiredPermissions = camera
                    && ((permissions & requiredPermissions) == requiredPermissions);

                if (hasRequiredPermissions == newHasRequiredPermissions)
                    return;

                hasRequiredPermissions = newHasRequiredPermissions;
                emit q->hasRequiredPermissionsChanged();
            };

        QObject::connect(accessNotifier.get(), &AccessController::Notifier::permissionsChanged,
            q, updateHasRequiredPermissions);
    }

    hasRequiredPermissions = accessController
        && ((accessController->permissions(camera) & requiredPermissions) == requiredPermissions);

    if (hasRequiredPermissions != hadRequiredPermissions)
        emit q->hasRequiredPermissionsChanged();
}

void BaseCameraButtonController::Private::cancelActiveActions()
{
    while (!activeActions.empty())
        q->cancelAction(*activeActions.cbegin());
}

//-------------------------------------------------------------------------------------------------

BaseCameraButtonController::BaseCameraButtonController(
    CameraButton::Group buttonGroup,
    Qn::Permissions requiredPermissions,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{.q = this, .buttonGroup = buttonGroup,
        .requiredPermissions = requiredPermissions })
{
    connect(this, &AbstractCameraButtonController::buttonChanged, this,
        [this](const CameraButton& button, CameraButton::Fields fields)
        {
            // Try to cancel action.
            if (fields.testFlag(CameraButton::Field::type))
                cancelAction(button.id);
        });
}

BaseCameraButtonController::~BaseCameraButtonController()
{
    d->cancelActiveActions();
}

void BaseCameraButtonController::setResourceInternal(const QnResourcePtr& resource)
{
    base_type::setResourceInternal(resource);
    d->setupCamera();
}

CameraButtons BaseCameraButtonController::buttons() const
{
    return d->buttons;
}

OptionalCameraButton BaseCameraButtonController::button(const nx::Uuid& buttonId) const
{
    const auto it = lowerBound(d->buttons, buttonId);
    return it != d->buttons.end() && it->id == buttonId
        ? OptionalCameraButton(*it)
        : OptionalCameraButton();
}

bool BaseCameraButtonController::startAction(const nx::Uuid& buttonId)
{
    const auto& targetButton = button(buttonId);
    if (!NX_ASSERT(targetButton, "Can't find button to activate!"))
        return false;

    if (!NX_ASSERT(!actionIsActive(buttonId), "Can't activate button twice!"))
        return false;

    return setButtonActionState(*targetButton, ActionState::active);
}

bool BaseCameraButtonController::stopAction(const nx::Uuid& buttonId)
{
    const auto& targetButton = button(buttonId);
    if (!NX_ASSERT(targetButton, "Can't find a button to activate an action!"))
        return false;

    if (!actionIsActive(buttonId))
        return false;

    return setButtonActionState(*targetButton, ActionState::inactive);
}

bool BaseCameraButtonController::cancelAction(const nx::Uuid& buttonId)
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

bool BaseCameraButtonController::actionIsActive(const nx::Uuid& buttonId) const
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

bool BaseCameraButtonController::removeButton(const nx::Uuid& buttonId)
{
    const auto it = lowerBound(d->buttons, buttonId);
    if (it == d->buttons.end() || it->id != buttonId)
        return false;

    if (actionIsActive(buttonId))
        cancelAction(buttonId);

    d->buttons.erase(it);
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

void BaseCameraButtonController::addActiveAction(const nx::Uuid& buttonId)
{
    if (NX_ASSERT(!actionIsActive(buttonId), "Can't activate button twice!"))
        d->activeActions.insert(buttonId);
}

void BaseCameraButtonController::removeActiveAction(const nx::Uuid& buttonId)
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
