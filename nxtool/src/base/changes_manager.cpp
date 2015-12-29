
#include "changes_manager.h"

#include <base/changeset.h>
#include <base/rtu_context.h>
#include <base/servers_finder.h>
#include <base/apply_changes_task.h>
#include <models/changes_progress_model.h>
#include <models/servers_selection_model.h>


namespace
{
    rtu::ApplyChangesTaskPtr createTask(rtu::RtuContext *context
        , const rtu::ChangesetPointer &changeset
        , rtu::ServersSelectionModel *selectionModel
        , rtu::ServersFinder *serversFinder)
    {
        const rtu::ApplyChangesTaskPtr task = 
            rtu::ApplyChangesTask::create(changeset, selectionModel->selectedServers());

        const auto ids = task->targetServerIds();
        const auto locker = task->id();
        selectionModel->lockItems(ids, task->minimizedText());

        typedef std::weak_ptr<rtu::ApplyChangesTask> TaskWeak;
        const TaskWeak weak = task;
        QObject::connect(task.get(), &rtu::ApplyChangesTask::changesApplied
            , context, [ids, locker, context, weak, selectionModel]() 
        {
            selectionModel->unlockItems(ids);

            if (weak.expired())
                return;

            const rtu::ApplyChangesTaskPtr task = weak.lock();
            context->applyTaskCompleted(task); 
        });

        QObject::connect(task.get(), &rtu::ApplyChangesTask::systemNameUpdated
            , selectionModel, &rtu::ServersSelectionModel::updateSystemNameInfo);
        QObject::connect(task.get(), &rtu::ApplyChangesTask::portUpdated
            , selectionModel, &rtu::ServersSelectionModel::updatePortInfo);
        QObject::connect(task.get(), &rtu::ApplyChangesTask::passwordUpdated
            , selectionModel, &rtu::ServersSelectionModel::updatePasswordInfo);
        QObject::connect(task.get(), &rtu::ApplyChangesTask::dateTimeUpdated
            , selectionModel, &rtu::ServersSelectionModel::updateTimeDateInfo);
        QObject::connect(task.get(), &rtu::ApplyChangesTask::extraInfoUpdated
            , selectionModel, &rtu::ServersSelectionModel::updateExtraInfo);

        QObject::connect(serversFinder, &rtu::ServersFinder::serverDiscovered
            , task.get(), &rtu::ApplyChangesTask::serverDiscovered);
        QObject::connect(serversFinder, &rtu::ServersFinder::serversRemoved
            , task.get(), &rtu::ApplyChangesTask::serversDisappeared);
        QObject::connect(serversFinder, &rtu::ServersFinder::unknownAdded
            , task.get(), &rtu::ApplyChangesTask::unknownAdded);
        return task;
    }
}

rtu::ChangesManager::ChangesManager(RtuContext *context
    , ServersSelectionModel *selectionModel
    , ServersFinder *serversFinder
    , QObject *parent)
    : QObject(parent)

    , m_context(context)
    , m_selectionModel(selectionModel)
    , m_serversFinder(serversFinder)
    , m_changesModel(new ChangesProgressModel(this))
    , m_changeset()
{
}

rtu::ChangesManager::~ChangesManager()
{
}

rtu::ChangesProgressModel *rtu::ChangesManager::changesProgressModel()
{
    return m_changesModel.get();
}

QObject *rtu::ChangesManager::changesProgressModelObject()
{
    return changesProgressModel();
}

QObject *rtu::ChangesManager::changeset()
{
    return getChangeset().get();
}

void rtu::ChangesManager::applyChanges()
{
    const ApplyChangesTaskPtr &task = 
        createTask(m_context, m_changeset, m_selectionModel, m_serversFinder);
    m_context->showProgressTask(task);
    clearChanges();
}

void rtu::ChangesManager::clearChanges()
{
    m_changeset.reset();
}

rtu::ChangesetPointer &rtu::ChangesManager::getChangeset()
{
    if (!m_changeset)
        m_changeset = Changeset::create();

    return m_changeset;
}

