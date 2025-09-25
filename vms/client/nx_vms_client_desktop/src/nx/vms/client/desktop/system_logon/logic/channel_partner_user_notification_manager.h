// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/pending_operation.h>
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
    void handleUserRemoved(const QnResourceList& resources);

private:
    inline bool notificationHasBeenClosed() const;
    inline bool userCanSeeNotification() const;
    inline bool systemHasHiddenUsers() const;

    void updateNotificationState();

private:
    nx::utils::ScopedConnections m_connections;
    nx::Uuid m_notificationId;
    std::unique_ptr<nx::utils::PendingOperation> m_checkUsersOperation;
    bool m_hasHiddenUsers = false; //< Cached value.
};

} // namespace nx::vms::client::desktop
