#include "client_recent_connections_manager.h"

#include <client_core/client_core_settings.h>

#include <ui/models/recent_user_connections_model.h>

QnClientRecentConnectionsManager::QnClientRecentConnectionsManager()
    : base_type()
    , m_unbound()
    , m_bound()
    , m_dataCache()
{
    NX_ASSERT(qnClientCoreSettings, Q_FUNC_INFO, "Core settings are empty");

    const auto coreSettingsHandler = [this](int id)
    {
        if (id == QnClientCoreSettings::RecentUserConnections)
            updateModelsData();
    };

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this, coreSettingsHandler);

    updateModelsData();
}

QnClientRecentConnectionsManager::~QnClientRecentConnectionsManager()
{}

void QnClientRecentConnectionsManager::addModel(QnRecentUserConnectionsModel* model)
{
    connect(model, &QnRecentUserConnectionsModel::systemNameChanged, this, [this, model]()
    {
        updateModelBinding(model);
    });

    const auto systemName = model->systemName();
    m_unbound.insert(model);
    if (!systemName.isEmpty())
        updateModelBinding(model);
}

void QnClientRecentConnectionsManager::removeModel(QnRecentUserConnectionsModel* model)
{
    if (m_unbound.remove(model))
        return;

    const auto systemName = model->systemName();
    const auto it = m_bound.find(systemName);
    const bool isFound = (it != m_bound.end());

    qDebug() << "Removing model for " << systemName;
//    NX_ASSERT(isFound, Q_FUNC_INFO, "Model has not been found"); //TODO: #ynikitenkov Investigate
    if (!isFound)
        return;

    m_bound.erase(it);
}

void QnClientRecentConnectionsManager::updateModelBinding(QnRecentUserConnectionsModel* model)
{
    const bool isCorrectSystemName = !model->systemName().isEmpty();
    NX_ASSERT(isCorrectSystemName, Q_FUNC_INFO
        , "System name for model can't be empty");
    if (!isCorrectSystemName)
        return;

    const auto itUnbound = m_unbound.find(model);
    const bool isUnbound = (itUnbound != m_unbound.end());
    const auto systemName = model->systemName();
    if (isUnbound)
    {
        m_unbound.erase(itUnbound);
    }
    else
    {
        const auto it = std::find(m_bound.cbegin(), m_bound.cend(), model);

        const bool isBoundAlready = (it != m_bound.cend());
//        NX_ASSERT(!isBoundAlready, Q_FUNC_INFO, "Model is bound"); //TODO: #ynikitenkov Investigate
        if (isBoundAlready)
            return;

        const bool modelWithSameSystem = m_bound.contains(systemName);
//        NX_ASSERT(!modelWithSameSystem, Q_FUNC_INFO
//            , "Model with the same system name exists"); //TODO: #ynikitenkov Investigate

        if (modelWithSameSystem)
            return;
    }

    m_bound.insert(systemName, model);
    model->updateData(m_dataCache.value(systemName));
}

void QnClientRecentConnectionsManager::updateModelsData()
{
    m_dataCache.clear();
    const auto lastConnectionsData = qnClientCoreSettings->recentUserConnections();
    for (const auto connectionDesc : lastConnectionsData)
    {
        m_dataCache[connectionDesc.systemName].append(connectionDesc);
    }
    for (const auto boundModel : m_bound)
    {
        const auto systemName = boundModel->systemName();
        boundModel->updateData(m_dataCache.value(systemName));
    }
}
