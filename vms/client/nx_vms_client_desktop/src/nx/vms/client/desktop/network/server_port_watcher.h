// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class ServerPortWatcher: public QObject, public SystemContextAware
{
    using base_type = QObject;

public:
    ServerPortWatcher(SystemContext* context, QObject* parent = nullptr);

    virtual ~ServerPortWatcher() override;

private:
    nx::utils::ScopedConnection m_serverConnection;
};

} // namespace nx::vms::client::desktop
