#include "client_recent_connections_manager.h"

#include <core/core_settings.h>

#include <ui/models/last_system_users_model.h>

QnClientRecentConnectionsManager::QnClientRecentConnectionsManager()
    : base_type()
    , m_unbound()
    , m_bound()
    , m_dataCache()
{
    NX_ASSERT(qnCoreSettings, Q_FUNC_INFO, "Core settings are empty");

    const auto coreSettingsHandler = [this](int id)
    {
        if (id == QnCoreSettings::LastSystemConnections)
            updateModelsData();
    };

    connect(qnCoreSettings, &QnCoreSettings::valueChanged, this, coreSettingsHandler);

    updateModelsData();
}

QnClientRecentConnectionsManager::~QnClientRecentConnectionsManager()
{}

void QnClientRecentConnectionsManager::addModel(QnLastSystemUsersModel* model)
{
    connect(model, &QnLastSystemUsersModel::systemNameChanged, this, [this, model]()
    {
        updateModelBinding(model);
    });

    const auto systemName = model->systemName();
    m_unbound.insert(model);
    if (!systemName.isEmpty())
        updateModelBinding(model);
}

void QnClientRecentConnectionsManager::removeModel(QnLastSystemUsersModel* model)
{
    if (m_unbound.remove(model))
        return;

    const auto systemName = model->systemName();
    const auto it = m_bound.find(systemName);
    const bool isFound = (it != m_bound.end());

    NX_ASSERT(isFound, Q_FUNC_INFO, "Model has not been found");
    if (!isFound)
        return;

    m_bound.erase(it);
}

void QnClientRecentConnectionsManager::updateModelBinding(QnLastSystemUsersModel* model)
{
    const bool isCorrectSystemName = !model->systemName().isEmpty();
    NX_ASSERT(isCorrectSystemName, Q_FUNC_INFO
        , "System name for model can't be empty");
    if (!isCorrectSystemName)
        return;

    const auto itUnbound = m_unbound.find(model);
    const bool isUnbound = (itUnbound != m_unbound.end());
    NX_ASSERT(isUnbound, Q_FUNC_INFO, "Model is not unbound");

    const auto systemName = model->systemName();
    if (isUnbound)
    {
        m_unbound.erase(itUnbound);
    }
    else
    {
        const auto it = std::find(m_bound.cbegin(), m_bound.cend(), model);

        const bool isBoundAlready = (it != m_bound.cend());
        NX_ASSERT(!isBoundAlready, Q_FUNC_INFO, "Model is bound");
        if (isBoundAlready)
            return;

        const bool modelWithSameSystem = m_bound.contains(systemName);
        NX_ASSERT(!modelWithSameSystem, Q_FUNC_INFO
            , "Model with the same system name exists");

        if (modelWithSameSystem)
            return;
    }

    m_bound.insert(systemName, model);
    model->updateData(m_dataCache.value(systemName));
}

void QnClientRecentConnectionsManager::updateModelsData()
{
    m_dataCache.clear();
    const auto lastConnectionsData = qnCoreSettings->lastSystemConnections();
    for (const auto connectionDesc : lastConnectionsData)
    {
        m_dataCache[connectionDesc.systemName].append(UserPasswordPair(
            connectionDesc.userName, connectionDesc.password));
    }
    for (const auto boundModel : m_bound)
    {
        const auto systemName = boundModel->systemName();
        boundModel->updateData(m_dataCache.value(systemName));
    }
}
