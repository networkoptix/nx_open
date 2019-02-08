
#include "changes_progress_model.h"

#include <base/apply_changes_task.h>
#include <helpers/model_change_helper.h>

namespace api = nx::vms::server::api;

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
    
        , kInProgreess = kFirstCustomRoleId
        , kTotalChangesCount
        , kAppliedChagnesCount
        , kErrorsCount
        , kOperationText
        
        , kLastCustomRoleId        
    };
    
    typedef QVector<int> RolesVector;

    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kInProgreess, "inProgress");
        roles.insert(kTotalChangesCount, "totalChangesCount");
        roles.insert(kAppliedChagnesCount, "appliedChangesCount");
        roles.insert(kErrorsCount, "errorsCount");
        roles.insert(kOperationText, "operationText");
        return roles;
    }();
}

rtu::ChangesProgressModel::ChangesProgressModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_changeHelper(CREATE_MODEL_CHANGE_HELPER(this))
    , m_tasks()
    , m_completedCount(0)
{
}

rtu::ChangesProgressModel::~ChangesProgressModel()
{
}

int rtu::ChangesProgressModel::completedCount() const
{
    return m_completedCount;
}

bool rtu::ChangesProgressModel::addChangeProgress(const ApplyChangesTaskPtr &task)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
        , [task](const ApplyChangesTaskPtr &item) { return (item == task); });

    if (it != m_tasks.end())
        return false;

    const auto itInsert = findInsertPosition();

    ApplyChangesTask * const taskPointer = task.get();
    QObject::connect(taskPointer, &ApplyChangesTask::totalChangesCountChanged
        , this, [this, taskPointer]() { taskStateChanged(taskPointer); });

    QObject::connect(taskPointer, &ApplyChangesTask::appliedChangesCountChanged
        , this, [this, taskPointer]() { taskStateChanged(taskPointer); });

    QObject::connect(taskPointer, &ApplyChangesTask::errorsCountChanged
        , this, [this, taskPointer]()
    {
        const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
            , [taskPointer](const ApplyChangesTaskPtr &item) { return (item.get() == taskPointer); });
        if (it == m_tasks.end())
            return;

        const int offset = (it - m_tasks.begin());
        const auto ind = index(offset);
        dataChanged(ind, ind, RolesVector(1, kErrorsCount));
    });

    const int insertIndex = itInsert - m_tasks.begin();
    const auto insertGuard = m_changeHelper->insertRowsGuard(insertIndex, insertIndex);
    Q_UNUSED(insertGuard);

    m_tasks.insert(itInsert, task);
    return true;
}

void rtu::ChangesProgressModel::removeByIndex(int index)
{
    if ((index < 0) || (index >= m_tasks.size()))
        return;

    const auto it = (m_tasks.begin() + index);
    QObject::disconnect(it->get(), nullptr, this, nullptr);
    if (!it->get()->inProgress())
    {
        --m_completedCount;
        completedCountChanged();
    }

    const auto removeGuard = m_changeHelper->removeRowsGuard(index, index);
    Q_UNUSED(removeGuard);
    m_tasks.erase(it);
}

void rtu::ChangesProgressModel::removeByTask(ApplyChangesTask *task)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
        , [task](const ApplyChangesTaskPtr &item) { return (item.get() == task); });

    if (it == m_tasks.end())
        return;

    removeByIndex(it - m_tasks.begin());
}

void rtu::ChangesProgressModel::serverDiscovered(const api::BaseServerInfo &info)
{
    for (const auto &task: m_tasks)
        task->serverDiscovered(info);
}
        
void rtu::ChangesProgressModel::serversDisappeared(const IDsVector &ids)
{
    for (const auto &task: m_tasks)
        task->serversDisappeared(ids);
}

void rtu::ChangesProgressModel::unknownAdded(const QString &ip)
{
    for (const auto &task: m_tasks)
        task->unknownAdded(ip);
}

rtu::ApplyChangesTaskPtr rtu::ChangesProgressModel::atIndex(int index)
{
    if ((index < 0) || (index >= m_tasks.size()))
        return ApplyChangesTaskPtr();

    return m_tasks.at(index);
}

int rtu::ChangesProgressModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // Parent item should be root
        return 0;

    return static_cast<int>(m_tasks.size());
}

QVariant rtu::ChangesProgressModel::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();
    if ((row < 0) || (row >= rowCount()))
        return QVariant();

    const ApplyChangesTaskPtr &task = m_tasks.at(row);
    switch(role)
    {
    case kInProgreess:
        return task->inProgress(); 
    case kTotalChangesCount:
        return task->totalChangesCount();
    case kAppliedChagnesCount:
        return task->appliedChangesCount();
    case kErrorsCount:
        return task->errorsCount();
    case kOperationText:
        return task->minimizedText();
    default:
        return QVariant();
    }
}
        
rtu::Roles rtu::ChangesProgressModel::roleNames() const
{
    return kRoles;
}

void rtu::ChangesProgressModel::taskStateChanged(const ApplyChangesTask *task)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [task](const ApplyChangesTaskPtr &taskVal)
        { return (taskVal.get() == task); });

    if (it == m_tasks.end())
        return;

    const int row = (it - m_tasks.begin());
    const auto ind = index(row);
    
    dataChanged(ind, ind);

    if (task->inProgress())
        return;
    
    const auto itInsert = findInsertPosition();
    if ((itInsert == m_tasks.end()) || (itInsert < it))
    {
        /// Move completed task up
        const auto tmp = *it;
        m_tasks.erase(it);

        const auto itInsert = findInsertPosition();
        const int insertIndex = (itInsert - m_tasks.begin());
        m_tasks.insert(itInsert, tmp);

        m_changeHelper->dataChanged(insertIndex, rowCount() - 1);
    }

    ++m_completedCount;
    completedCountChanged();
}

rtu::ChangesProgressModel::TasksContainer::iterator rtu::ChangesProgressModel::findInsertPosition()
{
    const auto itInsert = std::lower_bound(m_tasks.begin(), m_tasks.end(), true
        , [this](const ApplyChangesTaskPtr &item, bool /* value */) { return !item->inProgress(); });

    return itInsert;
}
