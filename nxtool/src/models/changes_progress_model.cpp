
#include "changes_progress_model.h"

#include <base/apply_changes_task.h>
#include <helpers/model_change_helper.h>

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
    
        , kInProgreess = kFirstCustomRoleId
        , kTotalChangesCount
        , kAppliedChagnesCount
        
        , kLastCustomRoleId        
    };
    
    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kInProgreess, "inProgress");
        roles.insert(kTotalChangesCount, "totalChangesCount");
        roles.insert(kAppliedChagnesCount, "appliedChangesCount");
        return roles;
    }();
}

rtu::ChangesProgressModel::ChangesProgressModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_changeHelper(CREATE_MODEL_CHANGE_HELPER(this))
    , m_tasks()
{
}

rtu::ChangesProgressModel::~ChangesProgressModel()
{
}

void rtu::ChangesProgressModel::taskStateChanged(const ApplyChangesTask *task
    , int role)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [task](const ApplyChangesTaskPtr &taskVal)
    {
        return (taskVal.get() == task);
    });

    if (it == m_tasks.end())
        return;

    const int row = (it - m_tasks.begin());
    const auto ind = index(row);
    dataChanged(ind, ind);
}

bool rtu::ChangesProgressModel::addChangeProgress(const ApplyChangesTaskPtr &task)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
        , [task](const ApplyChangesTaskPtr &item)
    {
        return (item == task);
    });

    if (it != m_tasks.end())
        return false;

    const int insertIndex = m_tasks.size();
    const auto insertGuard = m_changeHelper->insertRowsGuard(insertIndex, insertIndex);
    Q_UNUSED(insertGuard);

    ApplyChangesTask * const taskPointer = task.get();
    QObject::connect(taskPointer, &ApplyChangesTask::totalChangesCountChanged
        , this, [this, taskPointer]() { taskStateChanged(taskPointer, kTotalChangesCount); });

    QObject::connect(taskPointer, &ApplyChangesTask::appliedChangesCountChanged
        , this, [this, taskPointer]() { taskStateChanged(taskPointer, kAppliedChagnesCount); });

    m_tasks.push_back(task);
    return true;
}

void rtu::ChangesProgressModel::removeChangeProgress(int index)
{
    if ((index < 0) || (index >= m_tasks.size()))
        return;

    const auto removeGuard = m_changeHelper->removeRowsGuard(index, index);
    Q_UNUSED(removeGuard);
    
    const auto it = (m_tasks.begin() + index);
    QObject::disconnect(it->get(), nullptr, this, nullptr);
    m_tasks.erase(it);
}

void rtu::ChangesProgressModel::remove(ApplyChangesTask *task)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
        , [task](const ApplyChangesTaskPtr &item)
    {
        return (item.get() == task);
    });

    if (it == m_tasks.end())
        return;

    removeChangeProgress(it - m_tasks.begin());
}

rtu::ApplyChangesTaskPtr rtu::ChangesProgressModel::atIndex(int index)
{
    if ((index < 0) || (index >= m_tasks.size()))
        return ApplyChangesTaskPtr();

    return m_tasks.at(index);
}


void rtu::ChangesProgressModel::serverDiscovered(const rtu::BaseServerInfo &info)
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

int rtu::ChangesProgressModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) // Parent item should be root
        return 0;

    return m_tasks.size();
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

    default:
        return QVariant();
    }
}
        
rtu::Roles rtu::ChangesProgressModel::roleNames() const
{
    return kRoles;
}