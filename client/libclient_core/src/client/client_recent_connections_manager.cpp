#include "client_recent_connections_manager.h"

#include <client_core/client_core_settings.h>

#include <ui/models/recent_user_connections_model.h>

#include <utils/common/scoped_value_rollback.h>

QnClientRecentConnectionsManager::QnClientRecentConnectionsManager():
    base_type(),
    m_models(),
    m_dataCache(),
    m_updating(false)
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
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);

    connect(model, &QnRecentUserConnectionsModel::systemNameChanged, this,
        [this, model]()
        {
            updateModelBinding(model);
        });

    const auto systemName = model->systemName();
    m_models << model;
    if (!systemName.isEmpty())
        updateModelBinding(model);
}

void QnClientRecentConnectionsManager::removeModel(QnRecentUserConnectionsModel* model)
{
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    NX_ASSERT(m_models.contains(model));

    const auto systemName = model->systemName();
    qDebug() << "Removing model for " << systemName;
    m_models.removeOne(model);
}

void QnClientRecentConnectionsManager::updateModelBinding(QnRecentUserConnectionsModel* model)
{
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    const auto systemName = model->systemName();
    const bool isCorrectSystemName = !systemName.isEmpty();
    NX_ASSERT(isCorrectSystemName, Q_FUNC_INFO, "System name for model can't be empty");
    if (!isCorrectSystemName)
        return;

    model->updateData(m_dataCache.value(systemName));
}

void QnClientRecentConnectionsManager::updateModelsData()
{
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    m_dataCache.clear();
    const auto lastConnectionsData = qnClientCoreSettings->recentUserConnections();
    for (const auto connectionDesc : lastConnectionsData)
    {
        m_dataCache[connectionDesc.systemId].append(connectionDesc);
    }
    for (const auto model : m_models)
    {
        NX_ASSERT(model);
        if (!model || model->systemName().isEmpty())
            continue;
        const auto systemName = model->systemName();
        model->updateData(m_dataCache.value(systemName));
    }
}
