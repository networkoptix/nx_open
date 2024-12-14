// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_camera_button_controller.h"

#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

void AbstractCameraButtonController::registerQmlType()
{
    qmlRegisterUncreatableType<AbstractCameraButtonController>("nx.vms.client.core", 1, 0,
        "AbstractCameraButtonController", "Can't create");
}

AbstractCameraButtonController::AbstractCameraButtonController(QObject* parent):
    base_type(parent)
{
}

QnResourcePtr AbstractCameraButtonController::resource() const
{
    return m_resource;
}

void AbstractCameraButtonController::setResource(const QnResourcePtr& value)
{
    if (m_resource == value)
        return;

    // Let descendant classes process resource change before the signal is emitted.
    setResourceInternal(value);

    emit resourceChanged();
}

void AbstractCameraButtonController::setResourceInternal(const QnResourcePtr& value)
{
    m_resource = value;
}

SystemContext* AbstractCameraButtonController::systemContext() const
{
    NX_ASSERT(m_resource);
    return SystemContext::fromResource(m_resource);
}

QnResource* AbstractCameraButtonController::rawResource() const
{
    return m_resource.data();
}

void AbstractCameraButtonController::setRawResource(QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

void AbstractCameraButtonController::safeEmitActionStarted(const nx::Uuid& id, bool success)
{
    executeLater(
        nx::utils::guarded(this,
            [this, id, success]()
            {
                const auto button = buttonData(id);
                if (!NX_ASSERT(button))
                    return;

                if (!success
                    || button->type != CameraButtonData::Type::prolonged
                    || actionIsActive(id)) //< Avoid excessive signal for the prolonged triggers.
                {
                    emit actionStarted(id, success, QPrivateSignal());
                }
            }),
        this);
}

void AbstractCameraButtonController::safeEmitActionStopped(const nx::Uuid& id, bool success)
{
    executeLater(
        nx::utils::guarded(this,
            [this, id, success]()
            {
                const auto button = buttonData(id);
                if (!NX_ASSERT(button))
                    return;

                if (!success
                    || button->type != CameraButtonData::Type::prolonged
                    || !actionIsActive(id)) //< Avoid excessive signal for the prolonged triggers.
                {
                    emit actionStopped(id, success, QPrivateSignal());
                }
            }),
        this);
}

void AbstractCameraButtonController::safeEmitActionCancelled(const nx::Uuid& id)
{
    executeLater(
        nx::utils::guarded(this, [=]() { emit actionCancelled(id, QPrivateSignal()); }),
        this);
}

} // namespace nx::vms::client::core
