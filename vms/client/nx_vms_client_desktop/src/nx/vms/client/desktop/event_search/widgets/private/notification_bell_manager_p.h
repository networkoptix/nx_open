// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/common/system_health/message_type.h>

namespace nx::vms::client::desktop {

class NotificationBellWidget;

class NotificationBellManager: public QObject
{
    Q_OBJECT

public:
    NotificationBellManager(NotificationBellWidget* widget, QObject* parent = nullptr);
    virtual ~NotificationBellManager();

    void setAlarmStateActive(bool active);

    void handleNotificationAdded(
        nx::vms::common::system_health::MessageType message,
        const QVariant& params);

    void handleNotificationRemoved(
        nx::vms::common::system_health::MessageType message,
        const QVariant& params);

private:
    void countNotification(
        nx::vms::common::system_health::MessageType message,
        const QVariant& params,
        bool add);

private:
    NotificationBellWidget* m_widget = nullptr;

    bool m_isActive = false;
    std::set<QnUuid> m_activeNotifications;
};

} // nx::vms::client::desktop
