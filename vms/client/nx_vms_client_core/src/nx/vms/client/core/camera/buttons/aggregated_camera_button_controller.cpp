// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_camera_button_controller.h"

#include <unordered_map>

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>

#include "camera_button_data.h"

namespace nx::vms::client::core {

using namespace nx::vms::api;

struct AggregatedCameraButtonController::Private
{
    AggregatedCameraButtonController * const q;
    bool initialized = false;

    QHash<int, ControllerPtr> controllers;
    std::unordered_map<nx::Uuid, ControllerPtr> activeActions;

    Private(AggregatedCameraButtonController * const q);
};

AggregatedCameraButtonController::Private::Private(AggregatedCameraButtonController * const q):
    q(q)
{
    connect(q, &AbstractCameraButtonController::resourceChanged, q,
        [this]()
        {
            for (const auto& controller: controllers)
                controller->setResource(this->q->resource());
        });
}

//-------------------------------------------------------------------------------------------------

AggregatedCameraButtonController::AggregatedCameraButtonController(QObject* parent):
    base_type(parent),
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
        [this, controller](const nx::Uuid& buttonId, bool success)
        {
            const auto& button = controller->buttonData(buttonId);
            if (!button->instant() && !success)
                d->activeActions.erase(buttonId);

            safeEmitActionStarted(buttonId, success);
        });

    connect(controller.get(), &AbstractCameraButtonController::actionStopped, this,
        [this](const nx::Uuid& buttonId, bool success)
        {
            if (d->activeActions.erase(buttonId))
                safeEmitActionStopped(buttonId, success);
        });

    connect(controller.get(), &AbstractCameraButtonController::actionCancelled, this,
        [this](const nx::Uuid& buttonId)
        {
            if (d->activeActions.erase(buttonId))
                safeEmitActionCancelled(buttonId);
        });

    controller->setResource(resource());
}

CameraButtonDataList AggregatedCameraButtonController::buttonsData() const
{
    CameraButtonDataList result;
    for (const auto& controller: d->controllers)
    {
        const auto& controllerButtons = controller->buttonsData();
        result.insert(result.end(), controllerButtons.cbegin(), controllerButtons.cend());
    }
    return result;
}

OptionalCameraButtonData AggregatedCameraButtonController::buttonData(const nx::Uuid& id) const
{
    for (const auto& controller: d->controllers)
    {
        if (const auto& result = controller->buttonData(id))
            return result;
    }

    NX_ASSERT(false, "Can't find camera button");
    return {};
}

bool AggregatedCameraButtonController::startAction(const nx::Uuid& buttonId)
{
    if (actionIsActive(buttonId))
        return false;

    for (const auto& controller: d->controllers)
    {
        if (const auto& button = controller->buttonData(buttonId))
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

bool AggregatedCameraButtonController::stopAction(const nx::Uuid& buttonId)
{
    const auto it = d->activeActions.find(buttonId);
    if (it == d->activeActions.end())
        return false;

    const auto controller = it->second;
    if (const auto& button = controller->buttonData(buttonId))
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

bool AggregatedCameraButtonController::cancelAction(const nx::Uuid& buttonId)
{
    const auto it = d->activeActions.find(buttonId);
    if (it == d->activeActions.end())
        return false;

    const auto controller = it->second;
    if (const auto& button = controller->buttonData(buttonId))
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

bool AggregatedCameraButtonController::actionIsActive(const nx::Uuid& buttonId) const
{
    return d->activeActions.contains(buttonId);
}

} // namespace nx::vms::client::core
