// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class TraffiRelayUrlWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit TraffiRelayUrlWatcher(SystemContext* context, QObject* parent = nullptr);
    virtual ~TraffiRelayUrlWatcher() override;

    QString trafficRelayUrl() const;

signals:
    void trafficRelayUrlReady();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
