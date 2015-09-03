
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
    dataChanged(ind, ind, QVector<int>(1, role));
}

void rtu::ChangesProgressModel::addChangeProgress(ApplyChangesTaskPtr &&task)
{
    const int insertIndex = m_tasks.size();
    const auto insertGuard = m_changeHelper->insertRowsGuard(insertIndex, insertIndex);
    Q_UNUSED(insertGuard);

    const ApplyChangesTask * const taskPtr = task.get();
    QObject::connect(taskPtr, &ApplyChangesTask::totalChangesCountChanged
        , this, [this, taskPtr]() { taskStateChanged(taskPtr, kTotalChangesCount); });

    QObject::connect(taskPtr, &ApplyChangesTask::appliedChangesCountChanged
        , this, [this, taskPtr]() { taskStateChanged(taskPtr, kAppliedChagnesCount); });

    m_tasks.push_back(std::move(task));
}

void rtu::ChangesProgressModel::removeChangeProgress(QObject *object)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end()
        , [object](const ApplyChangesTaskPtr &item) { return (item.get() == object);});
    if (it == m_tasks.end())
        return;

    const int index = (it - m_tasks.begin());
    const auto removeGuard = m_changeHelper->removeRowsGuard(index, index);
    Q_UNUSED(removeGuard);
    
    ApplyChangesTask *task = it->get();
    QObject::disconnect(task, nullptr, this, nullptr);
    if (task->inProgress())
    {
        it->release();
        task->setAutoremoveOnComplete();
    }
    m_tasks.erase(it);
}

QObject *rtu::ChangesProgressModel::taskAtIndex(int index)
{
    if ((index < 0) || (index >= rowCount()))
        return nullptr;

    return m_tasks.at(index).get();
}

void rtu::ChangesProgressModel::serverDiscovered(const rtu::BaseServerInfo &info)
{
    for (const auto &task: m_tasks)
    {
        task->serverDiscovered(info);
    }
}
        
void rtu::ChangesProgressModel::serversDisappeared(const IDsVector &ids)
{
    for (const auto &task: m_tasks)
    {
        task->serversDisappeared(ids);
    }
}

void rtu::ChangesProgressModel::unknownAdded(const QString &ip)
{
    for (const auto &task: m_tasks)
    {
        task->unknownAdded(ip);
    }
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
