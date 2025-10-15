// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

namespace nx::vms::client::mobile {

struct PushNotification;

class PushNotificationModel: public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        TitleRole = Qt::UserRole + 1,
        DescriptionRole,
        ImageRole,
        TimeRole,
        ViewedRole,
        UrlRole,
        CloudSystemIdRole,
    };

public:
    PushNotificationModel(QObject* parent = nullptr);
    virtual ~PushNotificationModel();
    void update();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role);
    virtual QHash<int, QByteArray> roleNames() const override;

    static void registerQmlType();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class PushNotificationFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList cloudSystemIds MEMBER m_cloudSystemIds NOTIFY cloudSystemIdsChanged)
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
    void cloudSystemIdsChanged();
    void filterChanged();

private:
    QStringList m_cloudSystemIds;
    Filter m_filter;
};

} // namespace nx::vms::client::mobile
