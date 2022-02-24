// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <health/system_health.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class SystemHealthState:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    SystemHealthState(QObject* parent = nullptr);
    virtual ~SystemHealthState() override;

    using SystemHealthIndex = QnSystemHealth::MessageType;
    bool state(SystemHealthIndex index) const;

    QVariant data(SystemHealthIndex index) const;

signals:
    void stateChanged(SystemHealthIndex index, bool state, QPrivateSignal);
    void dataChanged(SystemHealthIndex index, QPrivateSignal);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
