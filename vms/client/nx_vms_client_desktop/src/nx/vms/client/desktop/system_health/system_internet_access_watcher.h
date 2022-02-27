// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class SystemInternetAccessWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    explicit SystemInternetAccessWatcher(QObject* parent = nullptr);
    virtual ~SystemInternetAccessWatcher() override;

    bool systemHasInternetAccess() const;

signals:
    void internetAccessChanged(bool systemHasInternetAccess, QPrivateSignal);

private:
    void setHasInternetAccess(bool value);

private:
    bool m_hasInternetAccess = false;
};

} // namespace nx::vms::client::desktop
