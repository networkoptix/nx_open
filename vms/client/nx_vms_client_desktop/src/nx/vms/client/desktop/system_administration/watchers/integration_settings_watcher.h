// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class IntegrationSettingsWatcher: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    explicit IntegrationSettingsWatcher(SystemContext* context, QObject* parent = nullptr);

private:
    std::optional<Uuid> m_notificationId;
    utils::ScopedConnection m_connection;
};

} // namespace nx::vms::client::desktop
