// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_model.h"

#include <QtQml/QtQml>

#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>
#include <nx/vms/text/human_readable.h>

#include "push_notification_image_provider.h"

namespace nx::vms::client::mobile {

struct PushNotificationModel::Private
{
    QString user;
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

QString PushNotificationModel::user() const
{
    return d->user;
}

void PushNotificationModel::setUser(const QString& user)
{
    if (user == d->user)
        return;

    d->user = user;
    emit userChanged();

    update();
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

        case ViewedRole:
            return item.isRead;

        case UrlRole:
            return QUrl{QString::fromStdString(item.url)};

        case CloudSystemIdRole:
            return QString::fromStdString(item.cloudSystemId);
    }

    NX_ASSERT(false, "Unexpected role: %1", role);

    return {};
}

bool PushNotificationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= (int) d->notifications.size())
        return false;

    if (role == ViewedRole)
    {
        PushNotification& notification = d->notifications[index.row()];
        notification.isRead = value.toBool();

        appContext()->pushNotificationStorage()->setIsRead(
            d->user.toStdString(), notification.id, notification.isRead);

        emit dataChanged(index, index, {ViewedRole});
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
    result[ViewedRole] = "viewed";
    result[UrlRole] = "url";
    result[CloudSystemIdRole] = "cloudSystemId";
    return result;
}

void PushNotificationModel::update()
{
    emit beginResetModel();

    d->notifications = d->user.isEmpty()
        ? std::vector<PushNotification>{}
        : appContext()->pushNotificationStorage()->userNotifications(d->user.toStdString());

    emit endResetModel();
}

void PushNotificationModel::registerQmlType()
{
    qmlRegisterType<PushNotificationModel>("nx.vms.client.mobile", 1, 0, "PushNotificationModel");
}

bool PushNotificationFilterModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    using Roles = PushNotificationModel::Roles;
    const QModelIndex index = sourceModel()->index(row, /*column*/ 0, parent);

    auto hasFilterMatch =
        [&]()
        {
            const bool isViewedFilter = (m_filter == Filter::Viewed);
            return m_filter == Filter::All
                || index.data(Roles::ViewedRole).toBool() == isViewedFilter;
        };

    auto hasRegExpMatch =
        [&]()
        {
            const auto regExp = filterRegularExpression();
            return !regExp.isValid()
                || index.data(Roles::TitleRole).toString().contains(regExp)
                || index.data(Roles::DescriptionRole).toString().contains(regExp);
        };

    auto hasSystemMatch =
        [&]()
        {
            return m_cloudSystemIds.empty()
                || m_cloudSystemIds.contains(index.data(Roles::CloudSystemIdRole).toString());
        };

    return hasSystemMatch() && hasRegExpMatch() && hasFilterMatch();
}

PushNotificationFilterModel::PushNotificationFilterModel(QObject* parent):
    QSortFilterProxyModel(parent)
{
    connect(this, &PushNotificationFilterModel::rowsInserted,
        this, &PushNotificationFilterModel::countChanged);
    connect(this, &PushNotificationFilterModel::rowsRemoved,
        this, &PushNotificationFilterModel::countChanged);
    connect(this, &PushNotificationFilterModel::layoutChanged,
        this, &PushNotificationFilterModel::countChanged);
    connect(this, &PushNotificationFilterModel::modelReset,
        this, &PushNotificationFilterModel::countChanged);

    connect(this, &PushNotificationFilterModel::filterChanged,
        this, &PushNotificationFilterModel::invalidate);
    connect(this, &PushNotificationFilterModel::cloudSystemIdsChanged,
        this, &PushNotificationFilterModel::invalidate);
}

void PushNotificationFilterModel::registerQmlType()
{
    qmlRegisterType<PushNotificationFilterModel>(
        "nx.vms.client.mobile", 1, 0, "PushNotificationFilterModel");
}

} // namespace nx::vms::client::mobile
