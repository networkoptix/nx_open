// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <nx/utils/uuid.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::workbench {
class LocalNotificationsManager;
}

namespace nx::vms::client::desktop {

class TemporaryUserExpirationWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit TemporaryUserExpirationWatcher(QObject* parent);

private:
    void createNotification();
    void updateNotification();

private:
    QPointer<workbench::LocalNotificationsManager> m_notificationManager;
    std::optional<QnUuid> m_notification;
    QTimer m_timer;
};

} // namespace nx::vms::client::desktop
