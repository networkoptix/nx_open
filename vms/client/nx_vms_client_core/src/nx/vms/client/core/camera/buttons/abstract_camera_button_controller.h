// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "camera_button.h"

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::core {

class SystemContext;

/**
 * Abstract camera button controller interface class. Declares the interface of the buttons,
 * related acttions and interaction rules.
 */
class NX_VMS_CLIENT_CORE_API AbstractCameraButtonController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)

public:
    static void registerQmlType();

    AbstractCameraButtonController(QObject* parent = nullptr);

    /** Returns list of the available camera buttons. */
    virtual CameraButtons buttons() const = 0;

    /** Returns camera button description with specified identifier. */
    virtual OptionalCameraButton button(const nx::Uuid& buttonId) const = 0;

    /**
     * Tries to starts action for the specified button. Returns whether the start command was
     * successful.
     * When action is actually started 'actionStarted' signal should be emitted after current
     * function call is finished and when the action is actually started, using
     * safeEmitActionStarted function call.
     */
    Q_INVOKABLE virtual bool startAction(const nx::Uuid& buttonId) = 0;

    /**
     * Tries to stop action for the specified button. Returns whether the stop command was
     * successful.
     * Should emit 'actionStopped' after current function call is finished and when the action
     * is actually stopped, using safeEmitActionStopped function call.
     */
    Q_INVOKABLE virtual bool stopAction(const nx::Uuid& buttonId) = 0;

    /**
     * Tries to cancel action for the specified button. Returns whether action can be cancelled.
     * Before action cancellation tries to stop it.
     * Should emit 'actionCancelled' after current function call is finished when the action
     * is actually cancelled, using safeEmitActionCancelled function call.
     */
    Q_INVOKABLE virtual bool cancelAction(const nx::Uuid& buttonId) = 0;

    /** Returns wheter the action for the specified button is active. */
    Q_INVOKABLE virtual bool actionIsActive(const nx::Uuid& buttonId) const = 0;

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

protected:
    virtual void setResourceInternal(const QnResourcePtr& value);

    /** Current System Context. Helper function. Shall not be called when resource is not set */
    SystemContext* systemContext() const;

    void safeEmitActionStarted(const nx::Uuid& buttonId, bool success);
    void safeEmitActionStopped(const nx::Uuid& buttonId, bool success);
    void safeEmitActionCancelled(const nx::Uuid& buttonId);

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

signals:
    void resourceChanged();

    void buttonAdded(const CameraButton& button);
    void buttonChanged(const CameraButton& button, CameraButton::Fields fields);
    void buttonRemoved(const nx::Uuid& buttonId);

    /** Emitted when action for the specified button is strated with some result. */
    void actionStarted(const nx::Uuid& buttonId, bool success, QPrivateSignal);

    /** Emitted when action for the specified button is stooped with some result. */
    void actionStopped(const nx::Uuid& buttonId, bool success, QPrivateSignal);

    /** Emitted when action for the specified button is cancelled. */
    void actionCancelled(const nx::Uuid& buttonId, QPrivateSignal);

private:
    QnResourcePtr m_resource;
};

} // namespace nx::vms::client::core
