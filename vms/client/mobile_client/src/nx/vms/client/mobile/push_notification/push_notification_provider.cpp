// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_provider.h"

#include <ranges>

#include <QtQml/QtQml>

#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

namespace nx::vms::client::mobile {

PushNotificationProvider::PushNotificationProvider(QObject* parent):
    QObject(parent)
{
    connect(this, &PushNotificationProvider::userChanged,
        this, &PushNotificationProvider::update);
    connect(this, &PushNotificationProvider::cloudSystemIdsChanged,
        this, &PushNotificationProvider::update);
}

void PushNotificationProvider::update()
{
    const auto userNotifications = m_user.isEmpty()
        ? std::vector<PushNotification>{}
        : appContext()->pushNotificationStorage()->userNotifications(m_user.toStdString());

    auto notifications = userNotifications | std::ranges::views::filter(
        [this](const PushNotification& item)
        {
            return !m_cloudSystemIds.isValid()
                || m_cloudSystemIds.toStringList().contains(item.cloudSystemId);
        });

    m_notifications = QVector<PushNotification>{notifications.begin(), notifications.end()};

    m_unviewedCount = std::ranges::count_if(m_notifications,
        [](const PushNotification& item) { return !item.isRead; });

    emit updated();
}

void PushNotificationProvider::setViewed(const QString& id, bool value)
{
    appContext()->pushNotificationStorage()->setIsRead(
        m_user.toStdString(), id.toStdString(), value);
}

void PushNotificationProvider::registerQmlType()
{
    qmlRegisterType<PushNotificationProvider>(
        "nx.vms.client.mobile", 1, 0, "PushNotificationProvider");
}

} // namespace nx::vms::client::mobile
