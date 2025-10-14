// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_model.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>
#include <nx/vms/text/human_readable.h>

#include "push_notification_image_provider.h"

namespace nx::vms::client::mobile {

struct PushNotificationModel::Private
{
    std::vector<PushNotification> notifications;
};

PushNotificationModel::PushNotificationModel(QObject* parent):
    QAbstractListModel(parent),
    d(new Private{})
{
    update();
}

PushNotificationModel::~PushNotificationModel()
{
}

int PushNotificationModel::rowCount(const QModelIndex&) const
{
    return d->notifications.size();
}

QVariant PushNotificationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= (int) d->notifications.size())
        return {};

    const auto& item = d->notifications[index.row()];

    switch (role)
    {
        case TitleRole:
            return QString::fromStdString(item.title);
        case DescriptionRole:
            return QString::fromStdString(item.description);
        case ImageRole:
            return item.imageId.empty()
                ? ""
                : PushNotificationImageProvider::url(QString::fromStdString(item.imageId));
        case TimeRole:
        {
            using namespace text;

            const auto elapsed =
                std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch()) - item.time;

            const auto result = HumanReadable::timeSpan(
                elapsed,
                HumanReadable::AllTimeUnits,
                HumanReadable::SuffixFormat::Long,
                /*separator*/ {},
                HumanReadable::kAlwaysSuppressSecondUnit);

            return tr("%1 ago", "notification time, like '1 min ago'").arg(result);
        }

        case ReadRole:
            return item.isRead;
    }

    NX_ASSERT(false, "Unexpected role: %1", role);

    return {};
}

bool PushNotificationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= (int) d->notifications.size())
        return false;

    if (role == ReadRole)
    {
        PushNotification& notification = d->notifications[index.row()];
        notification.isRead = value.toBool();

        const auto& user = appContext()->cloudStatusWatcher()->cloudLogin().toStdString();
        appContext()->pushNotificationStorage()->setIsRead(
            user, notification.id, notification.isRead);

        emit dataChanged(index, index, {ReadRole});
        return true;
    }

    NX_ASSERT(false, "Unexpected role: %1", role);
    return QAbstractListModel::setData(index, value, role);
}

QHash<int, QByteArray> PushNotificationModel::roleNames() const
{
    auto result = QAbstractListModel::roleNames();
    result[TitleRole] = "title";
    result[DescriptionRole] = "description";
    result[ImageRole] = "image";
    result[TimeRole] = "time";
    result[ReadRole] = "read";
    return result;
}

void PushNotificationModel::update()
{
    emit beginResetModel();

    const auto& user = appContext()->cloudStatusWatcher()->cloudLogin();
    d->notifications = user.isEmpty()
        ? std::vector<PushNotification>{}
        : appContext()->pushNotificationStorage()->userNotifications(user.toStdString());

    emit endResetModel();
}

void PushNotificationModel::registerQmlType()
{
    qmlRegisterType<PushNotificationModel>("nx.vms.client.mobile", 1, 0, "PushNotificationModel");
}

} // namespace nx::vms::client::mobile
