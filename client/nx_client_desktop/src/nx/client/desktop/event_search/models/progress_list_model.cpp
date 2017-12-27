#include "progress_list_model.h"

#include <client/client_globals.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/ui/workbench/extensions/workbench_progress_manager.h>

namespace nx {
namespace client {
namespace desktop {

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
                NX_ASSERT(false, Q_FUNC_INFO, "Duplicate activityId");
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
                NX_ASSERT(false, Q_FUNC_INFO, "Non-existant activityId");
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
        this, changed({Qn::ProgressValueRole}));
    connect(manager, &WorkbenchProgressManager::descriptionChanged,
        this, changed({Qn::DescriptionTextRole}));
    connect(manager, &WorkbenchProgressManager::cancellableChanged,
        this, changed({Qn::RemovableRole}));
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
            return manager->progress(activityId);

        case Qn::DescriptionTextRole:
            return manager->description(activityId);

        case Qn::AnimatedRole:
            return true;

        default:
            return QVariant();
    }
}

bool ProgressListModel::setData(const QModelIndex& index, const QVariant& value, int role)
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

} // namespace desktop
} // namespace client
} // namespace nx
