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
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual QHash<int, QByteArray> roleNames() const override;

    static void registerQmlType();

signals:
    void notificationsChanged();

private:
    QVector<PushNotification> m_notifications;
};

/**
 * Proxy model that filters notifications by viewed status and text search.
 *
 * Text filter (regex) is always active and matches against the notification title and description
 * regardless of the current viewed-status filter.
 *
 * Viewed-status filter has three modes: All, Viewed, and Unviewed. Additionally, filterExceptionId
 * may be used to exclude a notification from the viewed-status filtering.
 */
class PushNotificationFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(Filter filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString filterExceptionId
        READ filterExceptionId WRITE setFilterExceptionId NOTIFY filterExceptionIdChanged)

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

    Filter filter() const { return m_filter; }
    void setFilter(Filter value);

    QString filterExceptionId() const { return m_filterExceptionId; }
    void setFilterExceptionId(const QString& value);

    static void registerQmlType();

protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

signals:
    void filterChanged();
    void filterExceptionIdChanged();

private:
    Filter m_filter = Filter::All;
    QString m_filterExceptionId;
};

} // namespace nx::vms::client::mobile
