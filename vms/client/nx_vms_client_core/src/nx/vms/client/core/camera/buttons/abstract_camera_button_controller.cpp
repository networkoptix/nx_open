// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_camera_button_controller.h"

#include <QtQml/QtQml>

#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

void AbstractCameraButtonController::registerQmlType()
{
    qmlRegisterUncreatableType<AbstractCameraButtonController>("nx.vms.client.core", 1, 0,
        "AbstractCameraButtonController", "Can't create");
}

AbstractCameraButtonController::AbstractCameraButtonController(
    SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(context)
{
}

AbstractCameraButtonController::AbstractCameraButtonController(
    std::unique_ptr<nx::vms::common::SystemContextInitializer> contextInitializer,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(std::move(contextInitializer))
{
}

nx::Uuid AbstractCameraButtonController::resourceId() const
{
    return m_resourceId;
}

void AbstractCameraButtonController::setResourceId(const nx::Uuid& value)
{
    if (m_resourceId == value)
        return;

    m_resourceId = value;
    emit resourceIdChanged();
}

void AbstractCameraButtonController::safeEmitActionStarted(const nx::Uuid& id, bool success)
{
    executeLater(
        nx::utils::guarded(this, [=]() { emit actionStarted(id, success, QPrivateSignal()); }),
        this);
}

void AbstractCameraButtonController::safeEmitActionStopped(const nx::Uuid& id, bool success)
{
    executeLater(
        nx::utils::guarded(this, [=]() { emit actionStopped(id, success, QPrivateSignal()); }),
        this);
}

void AbstractCameraButtonController::safeEmitActionCancelled(const nx::Uuid& id)
{
    executeLater(
        nx::utils::guarded(this, [=]() { emit actionCancelled(id, QPrivateSignal()); }),
        this);
}

} // namespace nx::vms::client::core
