// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/common/system_health/message_type.h>

namespace nx::vms::client::desktop {

class SystemHealthState:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    SystemHealthState(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~SystemHealthState() override;

    using SystemHealthIndex = common::system_health::MessageType;
    bool state(SystemHealthIndex index) const;

    QVariant data(SystemHealthIndex index) const;

signals:
    void stateChanged(SystemHealthIndex index, bool state, QPrivateSignal);
    void dataChanged(SystemHealthIndex index, QPrivateSignal);

private:
    class Private;
    const nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
