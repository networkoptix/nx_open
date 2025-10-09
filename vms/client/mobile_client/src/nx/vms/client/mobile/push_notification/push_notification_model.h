// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

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
        ReadRole,
        UrlRole,
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

} // namespace nx::vms::client::mobile
