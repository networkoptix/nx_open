// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

namespace nx::vms::client::mobile {

class PushNotificationModel: public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVector<PushNotification> notifications
        READ notifications WRITE setNotifications NOTIFY notificationsChanged)

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        SourceRole,
        ImageRole,
        TimeRole,
        ViewedRole,
        UrlRole,
        CloudSystemIdRole,
    };

public:
    PushNotificationModel(QObject* parent = nullptr);

    QVector<PushNotification> notifications() const { return m_notifications; };
    void setNotifications(const QVector<PushNotification>& notifications);

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role);
    virtual QHash<int, QByteArray> roleNames() const override;

    static void registerQmlType();

signals:
    void notificationsChanged();

private:
    QVector<PushNotification> m_notifications;
};

class PushNotificationFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(Filter filter MEMBER m_filter NOTIFY filterChanged)

public:
    enum Filter
    {
        All,
        Viewed,
        Unviewed,
    };
    Q_ENUM(Filter)

public:
    PushNotificationFilterModel(QObject* parent = nullptr);
    static void registerQmlType();

protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

signals:
    void filterChanged();

private:
    Filter m_filter;
};

} // namespace nx::vms::client::mobile
