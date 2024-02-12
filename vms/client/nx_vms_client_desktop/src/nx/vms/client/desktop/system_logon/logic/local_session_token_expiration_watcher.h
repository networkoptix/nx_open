// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop::workbench { class LocalNotificationsManager; }

namespace nx::vms::client::desktop {

class LocalSessionTokenExpirationWatcher: public QObject
{
    Q_OBJECT

public:
    LocalSessionTokenExpirationWatcher(
        QPointer<workbench::LocalNotificationsManager> notificationManager,
        QObject* parent);

signals:
    void authenticationRequested();

private:
    void notify(std::chrono::minutes timeLeft);
    void setNotificationTimeLeft(std::chrono::minutes timeLeft);

private:
    QPointer<workbench::LocalNotificationsManager> m_notificationManager;
    std::optional<nx::Uuid> m_notification;
};

} // namespace nx::vms::client::desktop
