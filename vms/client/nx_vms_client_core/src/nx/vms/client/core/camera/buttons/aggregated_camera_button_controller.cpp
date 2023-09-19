// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_camera_button_controller.h"

#include <unordered_map>

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>

#include "camera_button.h"

namespace nx::vms::client::core {

using namespace nx::vms::api;

struct AggregatedCameraButtonController::Private
{
    AggregatedCameraButtonController * const q;
    bool initialized = false;

    QHash<int, ControllerPtr> controllers;
    std::unordered_map<QnUuid, ControllerPtr> activeActions;

    Private(AggregatedCameraButtonController * const q);
};

AggregatedCameraButtonController::Private::Private(AggregatedCameraButtonController * const q):
    q(q)
{
    connect(q, &AbstractCameraButtonController::resourceIdChanged, q,
        [this]()
        {
            for (const auto& controller: controllers)
                controller->setResourceId(this->q->resourceId());
        });
}

//-------------------------------------------------------------------------------------------------

AggregatedCameraButtonController::AggregatedCameraButtonController(
    SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent),
    d(new Private(this))
{
}

AggregatedCameraButtonController::AggregatedCameraButtonController(
    std::unique_ptr<nx::vms::common::SystemContextInitializer> contextInitializer,
    QObject* parent)
    :
    base_type(std::move(contextInitializer), parent),
    d(new Private(this))
{
}

AggregatedCameraButtonController::~AggregatedCameraButtonController()
{
}

void AggregatedCameraButtonController::addController(int group, const ControllerPtr& controller)
{
    const auto it = d->controllers.find(group);
    if (!NX_ASSERT(it == d->controllers.cend()))
        return;

    d->controllers.insert(group, controller);

    connect(controller.get(), &AbstractCameraButtonController::buttonAdded,
        this, &AbstractCameraButtonController::buttonAdded);
    connect(controller.get(), &AbstractCameraButtonController::buttonRemoved,
        this, &AbstractCameraButtonController::buttonRemoved);
    connect(controller.get(), &AbstractCameraButtonController::buttonChanged,
        this, &AbstractCameraButtonController::buttonChanged);

    connect(controller.get(), &AbstractCameraButtonController::actionStarted, this,
        [this, controller](const QnUuid& buttonId, bool success)
        {
            const auto& button = controller->button(buttonId);
            if (!button->instant() && !success)
                d->activeActions.erase(buttonId);

            safeEmitActionStarted(buttonId, success);
        });

    connect(controller.get(), &AbstractCameraButtonController::actionStopped, this,
        [this](const QnUuid& buttonId, bool success)
        {
            if (d->activeActions.erase(buttonId))
                safeEmitActionStopped(buttonId, success);
        });

    connect(controller.get(), &AbstractCameraButtonController::actionCancelled, this,
        [this](const QnUuid& buttonId)
        {
            if (d->activeActions.erase(buttonId))
                safeEmitActionCancelled(buttonId);
        });

    controller->setResourceId(resourceId());
}

CameraButtons AggregatedCameraButtonController::buttons() const
{
    CameraButtons result;
    for (const auto& controller: d->controllers)
    {
        const auto& controllerButtons = controller->buttons();
        result.insert(result.end(), controllerButtons.cbegin(), controllerButtons.cend());
    }
    return result;
}

OptionalCameraButton AggregatedCameraButtonController::button(const QnUuid& id) const
{
    for (const auto& controller: d->controllers)
    {
        if (const auto& result = controller->button(id))
            return result;
    }

    NX_ASSERT(false, "Can't find camera button");
    return {};
}

bool AggregatedCameraButtonController::startAction(const QnUuid& buttonId)
{
    if (actionIsActive(buttonId))
        return false;

    for (const auto& controller: d->controllers)
    {
        if (const auto& button = controller->button(buttonId))
        {
            const bool result = controller->startAction(buttonId);
            if (result && !button->instant())
                d->activeActions.emplace(buttonId, controller);

            return result;
        }
    }

    NX_ASSERT(false, "Can't find controller for the button!");
    return false;
}

bool AggregatedCameraButtonController::stopAction(const QnUuid& buttonId)
{
    const auto it = d->activeActions.find(buttonId);
    if (it == d->activeActions.end())
        return false;

    const auto controller = it->second;
    if (const auto& button = controller->button(buttonId))
    {
        if (button->instant())
        {
            NX_ASSERT(false, "Can't deactivate instant trigger!");
            return false;
        }

        if (controller->stopAction(buttonId))
            return true;

        d->activeActions.erase(buttonId);
        return false;
    }

    NX_ASSERT(false, "Can't find the button!");
    return false;
}

bool AggregatedCameraButtonController::cancelAction(const QnUuid& buttonId)
{
    const auto it = d->activeActions.find(buttonId);
    if (it == d->activeActions.end())
        return false;

    const auto controller = it->second;
    if (const auto& button = controller->button(buttonId))
    {
        if (button->instant())
        {
            NX_ASSERT(false, "Can't cancel an instant trigger action!");
            return false;
        }

        if (!NX_ASSERT(controller->cancelAction(buttonId)))
        {
            // Something went wrong, but we have active action - we should remove it.
            d->activeActions.erase(buttonId);
            safeEmitActionCancelled(buttonId);
        }
    }

    NX_ASSERT(false, "Can't find the button!");
    return false;
}

bool AggregatedCameraButtonController::actionIsActive(const QnUuid& buttonId) const
{
    return d->activeActions.contains(buttonId);
}

} // namespace nx::vms::client::core
