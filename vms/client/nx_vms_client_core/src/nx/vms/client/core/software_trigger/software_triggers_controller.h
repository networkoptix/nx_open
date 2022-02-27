// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::core {

class SoftwareTriggersController: public QObject, public RemoteConnectionAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId
        NOTIFY resourceIdChanged)

public:
    SoftwareTriggersController(QObject* parent = nullptr);
    virtual ~SoftwareTriggersController() override;

    static void registerQmlType();

    QnUuid resourceId() const;
    void setResourceId(const QnUuid& id);

    Q_INVOKABLE bool activateTrigger(const QnUuid& ruleId);
    Q_INVOKABLE bool deactivateTrigger();
    Q_INVOKABLE void cancelTriggerAction();
    Q_INVOKABLE QnUuid activeTriggerId() const;
    Q_INVOKABLE bool hasActiveTrigger() const;

signals:
    void resourceIdChanged();

    void triggerActivated(const QnUuid& ruleId, bool success);
    void triggerDeactivated(const QnUuid& ruleId, bool success);
    void triggerCancelled(const QnUuid& ruleId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
