
#include "changes_manager.h"

#include <base/rtu_context.h>
#include <base/apply_changes_task.h>
#include <models/changes_progress_model.h>

namespace
{
    rtu::ApplyChangesTaskPtr createTask(rtu::RtuContext *context
        , rtu::HttpClient *httpClient
        , rtu::ServersSelectionModel *selectionModel
        , rtu::ChangesManager *changesManager)
    {
        rtu::ApplyChangesTaskPtr result(new rtu::ApplyChangesTask(context, httpClient, selectionModel, changesManager));
        rtu::ApplyChangesTask *task = result.get();

        QObject::connect(task, &rtu::ApplyChangesTask::changesApplied
            , [context, task]() { context->applyTaskCompleted(task); });

        return std::move(result);
    }
}

rtu::ChangesManager::ChangesManager(RtuContext *context
    , HttpClient *httpClient
    , ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)

    , m_context(context)
    , m_httpClient(httpClient)
    , m_selectionModel(selectionModel)
    , m_changesModel(new ChangesProgressModel(this))
    , m_currentTask()
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

rtu::ApplyChangesTask *rtu::ChangesManager::notMinimizedTask()
{
    return m_currentTask.get();
}

void rtu::ChangesManager::releaseNotMinimizedTaskOwning()
{
    if (m_currentTask)
        m_currentTask.release();
}

void rtu::ChangesManager::addSystemChange(const QString &systemName)
{
    changeTask()->addSystemChange(systemName);
}

void rtu::ChangesManager::addPasswordChange(const QString &password)
{
    changeTask()->addPasswordChange(password);
}

void rtu::ChangesManager::addPortChange(int port)
{
    changeTask()->addPortChange(port);
}

void rtu::ChangesManager::addDHCPChange(const QString &name
    , bool useDHCP)
{
    changeTask()->addDHCPChange(name, useDHCP);
}

void rtu::ChangesManager::addAddressChange(const QString &name
    , const QString &address)
{
    changeTask()->addAddressChange(name, address);
}

void rtu::ChangesManager::addMaskChange(const QString &name
    , const QString &mask)
{
    changeTask()->addMaskChange(name, mask);
}

void rtu::ChangesManager::addDNSChange(const QString &name
    , const QString &dns)
{
    changeTask()->addDNSChange(name, dns);
}

void rtu::ChangesManager::addGatewayChange(const QString &name
    , const QString &gateway)
{
    changeTask()->addGatewayChange(name, gateway);
}

void rtu::ChangesManager::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    changeTask()->addDateTimeChange(date, time, timeZoneId);
}

void rtu::ChangesManager::turnOnDhcp()
{
    changeTask()->turnOnDhcp();
}

void rtu::ChangesManager::applyChanges()
{
    const ApplyChangesTaskPtr &task = changeTask();
    task->applyChanges();

    m_context->showDetailsForTask(task.get());
}

void rtu::ChangesManager::minimizeProgress()
{
    if (m_currentTask)
        m_changesModel->addChangeProgress(std::move(m_currentTask));
}

void rtu::ChangesManager::clearChanges()
{
    m_currentTask.reset();
}

void rtu::ChangesManager::serverDiscovered(const rtu::BaseServerInfo &info)
{
    if (m_currentTask)
        m_currentTask->serverDiscovered(info);
    m_changesModel->serverDiscovered(info);
}

void rtu::ChangesManager::serversDisappeared(const IDsVector &ids)
{
    if (m_currentTask)
        m_currentTask->serversDisappeared(ids);
    m_changesModel->serversDisappeared(ids);
}

void rtu::ChangesManager::unknownAdded(const QString &ip)
{
    if (m_currentTask)
        m_currentTask->unknownAdded(ip);

    m_changesModel->unknownAdded(ip);
}

rtu::ApplyChangesTaskPtr &rtu::ChangesManager::changeTask()
{
    if (!m_currentTask)
        m_currentTask = createTask(m_context, m_httpClient, m_selectionModel, this);

    return m_currentTask;
}

