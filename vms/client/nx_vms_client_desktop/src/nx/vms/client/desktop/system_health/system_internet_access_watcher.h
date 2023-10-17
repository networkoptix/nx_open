// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class SystemInternetAccessWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit SystemInternetAccessWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~SystemInternetAccessWatcher() override;

    void start();

    bool systemHasInternetAccess() const;

signals:
    void internetAccessChanged(bool systemHasInternetAccess, QPrivateSignal);

private:
    void setHasInternetAccess(bool value);

private:
    bool m_hasInternetAccess = false;
};

} // namespace nx::vms::client::desktop
