// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

namespace nx::vms::client::mobile {

class PushNotificationProvider: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString user MEMBER m_user NOTIFY userChanged)
    Q_PROPERTY(QVariant cloudSystemIds MEMBER m_cloudSystemIds NOTIFY cloudSystemIdsChanged)
    Q_PROPERTY(QVector<PushNotification> notifications MEMBER m_notifications NOTIFY updated)
    Q_PROPERTY(int unviewedCount MEMBER m_unviewedCount NOTIFY updated)

public:
    PushNotificationProvider(QObject* parent = nullptr);

    Q_INVOKABLE void update();
    Q_INVOKABLE void setViewed(const QString& id, bool value);

    static void registerQmlType();

signals:
    void updated();
    void userChanged();
    void cloudSystemIdsChanged();

private:
    QString m_user;
    QVariant m_cloudSystemIds;
    QVector<PushNotification> m_notifications;
    int m_unviewedCount = 0;
};

} // namespace nx::vms::client::mobile
