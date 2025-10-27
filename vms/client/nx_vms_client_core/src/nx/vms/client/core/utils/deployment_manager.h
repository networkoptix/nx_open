// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

class DeploymentManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DeploymentManager(SystemContext* context, QObject* parent = nullptr);

    virtual ~DeploymentManager() override;

    Q_INVOKABLE void deployNewServer();

private:
    struct Private;
    utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
