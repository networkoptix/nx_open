// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_model.h"

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/text/human_readable.h>

#include "push_notification_image_provider.h"

namespace nx::vms::client::mobile {

PushNotificationModel::PushNotificationModel(QObject* parent):
    QAbstractListModel(parent)
{
}

int PushNotificationModel::rowCount(const QModelIndex&) const
{
    return m_notifications.size();
}

QVariant PushNotificationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= (int) m_notifications.size())
        return {};

    const auto& item = m_notifications[index.row()];

    switch (role)
    {
        case IdRole:
            return QString::fromStdString(item.id);

        case TitleRole:
            return QString::fromStdString(item.title);

        case DescriptionRole:
            return QString::fromStdString(item.description);

        case ImageRole:
            return item.imageUrl.empty()
                ? ""
                : PushNotificationImageProvider::url(
                    QString::fromStdString(item.cloudSystemId),
                    QString::fromStdString(item.imageUrl));

        case SourceRole:
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(
                QString::fromStdString(item.cloudSystemId));

            return context ? context->systemDescription()->name() : "";
        }

        case TimeRole:
        {
            using namespace text;

            const auto elapsed =
                std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch()) - item.time;

            const auto result = HumanReadable::timeSpan(
                elapsed,
                HumanReadable::Seconds | HumanReadable::Minutes | HumanReadable::Hours
                    | HumanReadable::Days | HumanReadable::Weeks | HumanReadable::Months
                    | HumanReadable::Years,
                HumanReadable::SuffixFormat::Long,
                /*separator*/ {},
                HumanReadable::kAlwaysSuppressSecondUnit);

            return tr("%1 ago", "notification time, like '1 min ago'").arg(result);
        }

        case ViewedRole:
            return item.isRead;

        case UrlRole:
            return QUrl{QString::fromStdString(item.url)};
    }

    NX_ASSERT(false, "Unexpected role: %1", role);

    return {};
}

bool PushNotificationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= (int) m_notifications.size())
        return false;

    if (role == ViewedRole)
    {
        PushNotification& notification = m_notifications[index.row()];
        notification.isRead = value.toBool();
        emit dataChanged(index, index, {ViewedRole});
        return true;
    }

    NX_ASSERT(false, "Unexpected role: %1", role);
    return QAbstractListModel::setData(index, value, role);
}

QHash<int, QByteArray> PushNotificationModel::roleNames() const
{
    auto result = QAbstractListModel::roleNames();
    result[IdRole] = "id";
    result[TitleRole] = "title";
    result[DescriptionRole] = "description";
    result[SourceRole] = "source";
    result[ImageRole] = "image";
    result[TimeRole] = "time";
    result[ViewedRole] = "viewed";
    result[UrlRole] = "url";
    return result;
}

void PushNotificationModel::setNotifications(const QVector<PushNotification>& notifications)
{
    emit beginResetModel();
    m_notifications = notifications;
    emit endResetModel();

    emit notificationsChanged();
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

    return hasRegExpMatch() && hasFilterMatch();
}

PushNotificationFilterModel::PushNotificationFilterModel(QObject* parent):
    QSortFilterProxyModel(parent)
{
    connect(this, &PushNotificationFilterModel::filterChanged,
        this, &PushNotificationFilterModel::invalidate);
}

void PushNotificationFilterModel::registerQmlType()
{
    qmlRegisterType<PushNotificationFilterModel>(
        "nx.vms.client.mobile", 1, 0, "PushNotificationFilterModel");
}

} // namespace nx::vms::client::mobile
