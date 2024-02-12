// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API SoftwareTriggersController: public QObject, public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(nx::Uuid resourceId READ resourceId WRITE setResourceId
        NOTIFY resourceIdChanged)

public:
    SoftwareTriggersController(SystemContext* systemContext, QObject* parent = nullptr);
    SoftwareTriggersController(); //< QML constructor.
    virtual ~SoftwareTriggersController() override;

    static void registerQmlType();

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid& id);

    Q_INVOKABLE bool activateTrigger(const nx::Uuid& ruleId);
    Q_INVOKABLE bool deactivateTrigger();
    Q_INVOKABLE nx::Uuid activeTriggerId() const;
    Q_INVOKABLE bool hasActiveTrigger() const;

signals:
    void resourceIdChanged();

    void triggerActivated(const nx::Uuid& ruleId, bool success);
    void triggerDeactivated(const nx::Uuid& ruleId, bool success);
    void triggerCancelled(const nx::Uuid& ruleId);

private:
    void cancelTriggerAction();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
