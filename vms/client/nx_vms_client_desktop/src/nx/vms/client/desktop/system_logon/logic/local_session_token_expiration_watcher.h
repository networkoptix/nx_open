// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>

namespace nx::vms::client::desktop {


namespace workbench { class LocalNotificationsManager; }

class LocalSessionTokenExpirationWatcher: public QObject
{
    Q_OBJECT

public:
    LocalSessionTokenExpirationWatcher(
        QPointer<workbench::LocalNotificationsManager> notificationManager,
        QObject* parent = nullptr);

    void setTimeoutWatcher(core::RemoteSessionTimeoutWatcher* sessionTimeoutWatcher);

signals:
    void authenticationRequested();

private:
    void notify(std::chrono::minutes timeLeft);
    void setNotificationTimeLeft(std::chrono::minutes timeLeft);

private:
    QPointer<core::RemoteSessionTimeoutWatcher> m_sessionTimeoutWatcher;
    QPointer<workbench::LocalNotificationsManager> m_notificationManager;
    std::optional<nx::Uuid> m_notification;
};

} // namespace nx::vms::client::desktop
