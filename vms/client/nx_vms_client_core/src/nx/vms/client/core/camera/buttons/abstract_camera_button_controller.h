// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

#include "camera_button.h"

namespace nx::vms::client::core {

/**
 * Abstract camera button controller interface class. Declares the interface of the buttons,
 * related acttions and interaction rules.
 */
class NX_VMS_CLIENT_CORE_API AbstractCameraButtonController: public QObject,
    protected SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnUuid resourceId
        READ resourceId
        WRITE setResourceId
        NOTIFY resourceIdChanged)

public:
    static void registerQmlType();

    AbstractCameraButtonController(
        SystemContext* context,
        QObject* parent = nullptr);
    AbstractCameraButtonController(
        std::unique_ptr<nx::vms::common::SystemContextInitializer> contextInitializer,
        QObject* parent = nullptr);

    QnUuid resourceId() const;
    void setResourceId(const QnUuid& value);

    /** Returns list of the available camera buttons. */
    virtual CameraButtons buttons() const = 0;

    /** Returns camera button description with specified identifier. */
    virtual OptionalCameraButton button(const QnUuid& buttonId) const = 0;

    /**
     * Tries to starts action for the specified button. Returns whether the start command was
     * successful.
     * When action is actually started 'actionStarted' signal should be emitted after current
     * function call is finished and when the action is actually started, using
     * safeEmitActionStarted function call.
     */
    Q_INVOKABLE virtual bool startAction(const QnUuid& buttonId) = 0;

    /**
     * Tries to stop action for the specified button. Returns whether the stop command was
     * successful.
     * Should emit 'actionStopped' after current function call is finished and when the action
     * is actually stopped, using safeEmitActionStopped function call.
     */
    Q_INVOKABLE virtual bool stopAction(const QnUuid& buttonId) = 0;

    /**
     * Tries to cancel action for the specified button. Returns whether action can be cancelled.
     * Before action cancellation tries to stop it.
     * Should emit 'actionCancelled' after current function call is finished when the action
     * is actually cancelled, using safeEmitActionCancelled function call.
     */
    Q_INVOKABLE virtual bool cancelAction(const QnUuid& buttonId) = 0;

    /** Returns wheter the action for the specified button is active. */
    Q_INVOKABLE virtual bool actionIsActive(const QnUuid& buttonId) const = 0;

protected:
    void safeEmitActionStarted(const QnUuid& buttonId, bool success);
    void safeEmitActionStopped(const QnUuid& buttonId, bool success);
    void safeEmitActionCancelled(const QnUuid& buttonId);

signals:
    void resourceIdChanged();

    void buttonAdded(const CameraButton& button);
    void buttonChanged(const CameraButton& button, CameraButton::Fields fields);
    void buttonRemoved(const QnUuid& buttonId);

    /** Emitted when action for the specified button is strated with some result. */
    void actionStarted(const QnUuid& buttonId, bool success, QPrivateSignal);

    /** Emitted when action for the specified button is stooped with some result. */
    void actionStopped(const QnUuid& buttonId, bool success, QPrivateSignal);

    /** Emitted when action for the specified button is cancelled. */
    void actionCancelled(const QnUuid& buttonId, QPrivateSignal);

private:
    QnUuid m_resourceId;
};

} // namespace nx::vms::client::core
