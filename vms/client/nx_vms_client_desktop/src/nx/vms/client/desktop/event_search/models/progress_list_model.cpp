#include "progress_list_model.h"

#include <QAction>

#include <client/client_globals.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <ui/common/notification_levels.h>
#include <ui/style/skin.h>

namespace {

// Same as for other notifications.
static const auto kFailedDisplayTimeout = std::chrono::milliseconds(12500);

} // namespace

namespace nx::vms::client::desktop {

ProgressListModel::ProgressListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_activities(context()->instance<WorkbenchProgressManager>()->activities())
{
    const auto added =
        [this](const QnUuid& activityId)
        {
            if (m_activities.contains(activityId))
            {
                NX_ASSERT(false, "Duplicate activityId");
                return;
            }

            ScopedInsertRows insertRows(this, m_activities.size(), m_activities.size());
            m_activities.push_back(activityId);
        };

    const auto removed =
        [this](const QnUuid& activityId)
        {
            const auto index = m_activities.index_of(activityId);
            if (index < 0)
            {
                NX_ASSERT(false, "Non-existant activityId");
                return;
            }

            ScopedRemoveRows removeRows(this, index, index);
            m_activities.removeAt(index);
        };

    const auto changed =
        [this](QVector<int> roles = {})
        {
            return
                [this, roles](const QnUuid& activityId)
                {
                    const auto index = m_activities.index_of(activityId);
                    if (index < 0)
                        return;

                    const auto modelIndex = this->index(index);
                    emit dataChanged(modelIndex, modelIndex, roles);
                };
        };

    auto manager = context()->instance<WorkbenchProgressManager>();
    connect(manager, &WorkbenchProgressManager::added, this, added);
    connect(manager, &WorkbenchProgressManager::removed, this, removed);
    connect(manager, &WorkbenchProgressManager::progressChanged,
        this, changed({Qn::ProgressValueRole, Qt::DecorationRole, Qn::CommandActionRole, Qt::ForegroundRole, Qn::TimeoutRole}));
    connect(manager, &WorkbenchProgressManager::titleChanged,
        this, changed({Qt::DisplayRole}));
    connect(manager, &WorkbenchProgressManager::descriptionChanged,
        this, changed({Qn::DescriptionTextRole}));
    connect(manager, &WorkbenchProgressManager::cancellableChanged,
        this, changed({Qn::RemovableRole, Qn::TimeoutRole}));
    connect(manager, &WorkbenchProgressManager::actionChanged,
        this, changed({Qn::CommandActionRole}));
}

int ProgressListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_activities.size();
}

QVariant ProgressListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount())
    {
        return QVariant();
    }

    const auto manager = context()->instance<WorkbenchProgressManager>();
    const auto& activityId = m_activities[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return manager->title(activityId);

        case Qn::ProgressValueRole:
            {
                const auto progress = manager->progress(activityId);
                return progress == WorkbenchProgressManager::kCompletedProgressValue 
                    || progress == WorkbenchProgressManager::kFailedProgressValue
                        ? QVariant()
                        : QVariant(progress);
            }

        case Qn::DescriptionTextRole:
            return manager->description(activityId);

        case Qn::RemovableRole:
            return manager->isCancellable(activityId);

        case Qn::CommandActionRole:
            if (auto action = manager->action(activityId))
                if (manager->progress(activityId) == WorkbenchProgressManager::kCompletedProgressValue)
                    return QVariant::fromValue(manager->action(activityId));
            return QVariant();

        case Qt::DecorationRole:
            {
                const auto progress = manager->progress(activityId);
                if (progress == WorkbenchProgressManager::kCompletedProgressValue)
                    return qnSkin->pixmap("events/success_mark.svg"); // "events/success_mark.png" "standard_icons/message_box_success.png"
                else if (progress == WorkbenchProgressManager::kFailedProgressValue)
                    return qnSkin->pixmap("events/alert_yellow.png");
                return QVariant();
            }

        case Qt::ForegroundRole:
            return manager->progress(activityId) == WorkbenchProgressManager::kFailedProgressValue
                ? QnNotificationLevel::notificationTextColor(QnNotificationLevel::Value::ImportantNotification)
                : QVariant();

        case Qn::TimeoutRole:
            return manager->isCancellable(activityId)
                    && manager->progress(activityId)
                        == WorkbenchProgressManager::kFailedProgressValue
                ? QVariant::fromValue(kFailedDisplayTimeout)
                : QVariant();

        case Qn::AlternateColorRole:
            return true;

        default:
            return QVariant();
    }
}

bool ProgressListModel::setData(const QModelIndex& index, const QVariant& /*value*/, int role)
{
    if (!index.isValid() || index.model() != this || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount()
        || role != Qn::DefaultNotificationRole)
    {
        return false;
    }

    context()->instance<WorkbenchProgressManager>()->interact(m_activities[index.row()]);
    return true;
}

bool ProgressListModel::removeRows(int row, int count, const QModelIndex&)
{
    for (int i = row; i < row + count; ++i)
    {
        context()->instance<WorkbenchProgressManager>()->cancel(m_activities[i]);
    }
    return false;
}

} // namespace nx::vms::client::desktop
