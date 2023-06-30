// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_notifications_list_model.h"

#include <QAction>

#include <client/client_globals.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <ui/common/notification_levels.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {
namespace {

// Same as for other notifications.
static const auto kFailedDisplayTimeout = std::chrono::milliseconds(12500);

QPixmap levelDecoration(QnNotificationLevel::Value level)
{
    switch (level)
    {
        case QnNotificationLevel::Value::SuccessNotification:
            return qnSkin->pixmap("events/success_mark.png");
        case QnNotificationLevel::Value::ImportantNotification:
            return qnSkin->pixmap("events/alert_yellow.png");
        case QnNotificationLevel::Value::CriticalNotification:
            return qnSkin->pixmap("events/alert_red.png");
    }
    return {};
}

QPixmap progressDecoration(ProgressState progress)
{
    if (progress.isFailed())
        return qnSkin->pixmap("events/alert_yellow.png");
    if (progress.isCompleted())
        return qnSkin->pixmap("events/success_mark.svg");

    return {};
}

} // namespace

LocalNotificationsListModel::LocalNotificationsListModel(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context),
    m_notifications(context->localNotificationsManager()->notifications())
{
    const auto added =
        [this](const QnUuid& notificationId)
        {
            if (m_notifications.contains(notificationId))
            {
                NX_ASSERT(false, "Duplicate notificationId");
                return;
            }

            ScopedInsertRows insertRows(this, m_notifications.size(), m_notifications.size());
            m_notifications.push_back(notificationId);
        };

    const auto removed =
        [this](const QnUuid& notificationId)
        {
            const auto index = m_notifications.index_of(notificationId);
            if (index < 0)
            {
                NX_ASSERT(false, "Non-existent notificationId");
                return;
            }

            ScopedRemoveRows removeRows(this, index, index);
            m_notifications.removeAt(index);
        };

    const auto changed =
        [this](QVector<int> roles = {})
        {
            return
                [this, roles](const QnUuid& notificationId)
                {
                    const auto index = m_notifications.index_of(notificationId);
                    if (index < 0)
                        return;

                    const auto modelIndex = this->index(index);
                    emit dataChanged(modelIndex, modelIndex, roles);
                };
        };

    auto manager = windowContext()->localNotificationsManager();
    connect(manager, &workbench::LocalNotificationsManager::added, this, added);
    connect(manager, &workbench::LocalNotificationsManager::removed, this, removed);
    connect(manager, &workbench::LocalNotificationsManager::progressChanged,
        this, changed({Qn::ProgressValueRole, Qt::DecorationRole, Qn::CommandActionRole, Qt::ForegroundRole, Qn::TimeoutRole}));
    connect(manager, &workbench::LocalNotificationsManager::titleChanged,
        this, changed({Qt::DisplayRole}));
    connect(manager, &workbench::LocalNotificationsManager::descriptionChanged,
        this, changed({core::DescriptionTextRole}));
    connect(manager, &workbench::LocalNotificationsManager::iconChanged,
        this, changed({Qt::DecorationRole}));
    connect(manager, &workbench::LocalNotificationsManager::cancellableChanged,
        this, changed({Qn::RemovableRole, Qn::TimeoutRole}));
    connect(manager, &workbench::LocalNotificationsManager::actionChanged,
        this, changed({Qn::CommandActionRole}));
    connect(manager, &workbench::LocalNotificationsManager::progressFormatChanged,
        this, changed({Qn::ProgressFormatRole}));
    connect(manager, &workbench::LocalNotificationsManager::levelChanged,
        this, changed({Qt::ForegroundRole, Qt::DecorationRole}));
    connect(manager, &workbench::LocalNotificationsManager::additionalTextChanged,
        this, changed({Qn::AdditionalTextRole}));
    connect(manager, &workbench::LocalNotificationsManager::tooltipChanged,
        this, changed({Qt::ToolTipRole}));
    connect(manager, &workbench::LocalNotificationsManager::additionalActionChanged,
        this, changed({Qn::AdditionalActionRole}));
}

int LocalNotificationsListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_notifications.size();
}

QVariant LocalNotificationsListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount())
    {
        return QVariant();
    }

    const auto manager = windowContext()->localNotificationsManager();
    const auto& notificationId = m_notifications[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return manager->title(notificationId);

        case Qn::ProgressValueRole:
        {
            if (const auto progress = manager->progress(notificationId))
                return QVariant::fromValue(*progress);

            return QVariant();
        }

        case core::DescriptionTextRole:
            return manager->description(notificationId);

        case Qn::RemovableRole:
            return manager->isCancellable(notificationId);

        case Qn::CommandActionRole:
        {
            const auto progress = manager->progress(notificationId);
            const auto action = manager->action(notificationId);

            if (action && (!progress || progress->isCompleted()))
                return QVariant::fromValue(action);

            return QVariant();
        }

        case Qn::AdditionalActionRole:
        {
            const auto action = manager->additionalAction(notificationId);
            if (action)
                return QVariant::fromValue(action);

            return QVariant();
        }

        case Qt::DecorationRole:
        {
            if (const auto pixmap = manager->icon(notificationId); !pixmap.isNull())
                return pixmap;
            if (const auto progress = manager->progress(notificationId))
                return progressDecoration(*progress);

            return levelDecoration(manager->level(notificationId));
        }

        case Qt::ForegroundRole:
        {
            if (const auto progress = manager->progress(notificationId))
            {
                return progress->isFailed()
                    ? QnNotificationLevel::notificationTextColor(
                        QnNotificationLevel::Value::ImportantNotification)
                    : QVariant();
            }

            return QnNotificationLevel::notificationTextColor(manager->level(notificationId));
        }

        case Qn::TimeoutRole:
            return manager->isCancellable(notificationId)
                && manager->progress(notificationId)
                    == ProgressState::failed
                ? QVariant::fromValue(kFailedDisplayTimeout)
                : QVariant();

        case Qn::ProgressFormatRole:
            return QVariant::fromValue(manager->progressFormat(notificationId));

        case Qn::AlternateColorRole:
            return true;

        case Qn::NotificationLevelRole:
            return QVariant::fromValue(manager->level(notificationId));

        case Qn::AdditionalTextRole:
            return manager->additionalText(notificationId);

        case Qt::ToolTipRole:
            return manager->tooltip(notificationId);

        default:
            return QVariant();
    }
}

bool LocalNotificationsListModel::setData(const QModelIndex& index, const QVariant& /*value*/, int role)
{
    if (!index.isValid() || index.model() != this || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount()
        || role != core::DefaultNotificationRole)
    {
        return false;
    }

    workbenchContext()->instance<workbench::LocalNotificationsManager>()->interact(
        m_notifications[index.row()]);
    return true;
}

bool LocalNotificationsListModel::removeRows(int row, int count, const QModelIndex&)
{
    for (int i = row; i < row + count; ++i)
    {
        workbenchContext()->instance<workbench::LocalNotificationsManager>()->cancel(
            m_notifications[i]);
    }
    return false;
}

} // namespace nx::vms::client::desktop
