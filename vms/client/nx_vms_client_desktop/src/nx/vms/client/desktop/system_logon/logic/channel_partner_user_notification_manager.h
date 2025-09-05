// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class ChannelPartnerUserNotificationManager: public QObject, public WindowContextAware
{
    Q_OBJECT

public:
    ChannelPartnerUserNotificationManager(WindowContext* windowContext, QObject* parent = nullptr);

    void handleConnect();
    void handleDisconnect();

    void handleUserAdded(const QnResourceList& resources);
    void handleUserChanged();

private:
    bool checkBasicConstantVisibilityConditions() const;
    bool checkBasicMutableVisibilityConditions() const;
    void showNotification();

private:
    nx::utils::ScopedConnections m_connections;
    nx::Uuid m_notificationId;
};

} // namespace nx::vms::client::desktop
