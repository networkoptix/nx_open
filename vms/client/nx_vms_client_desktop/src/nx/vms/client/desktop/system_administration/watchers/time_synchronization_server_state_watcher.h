// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class TimeSynchronizationWidgetStore;

class TimeSynchronizationServerStateWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit TimeSynchronizationServerStateWatcher(
        SystemContext* systemContext,
        TimeSynchronizationWidgetStore* store,
        QObject* parent = nullptr);
    virtual ~TimeSynchronizationServerStateWatcher() override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
