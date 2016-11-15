
#include "client_recent_connections_manager.h"

#include <client_core/client_core_settings.h>

#include <ui/models/recent_local_connections_model.h>

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
            if (id == QnClientCoreSettings::RecentLocalConnections)
                updateModelsData();
        };

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this, coreSettingsHandler);

    updateModelsData();
}

QnClientRecentConnectionsManager::~QnClientRecentConnectionsManager()
{}

void QnClientRecentConnectionsManager::addModel(QnRecentLocalConnectionsModel* model)
{
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);

    connect(model, &QnRecentLocalConnectionsModel::systemIdChanged, this,
        [this, model]()
        {
            updateModelBinding(model);
        });

    const auto systemId = model->systemId();
    m_models << model;
    if (!systemId.isNull())
        updateModelBinding(model);
}

void QnClientRecentConnectionsManager::removeModel(QnRecentLocalConnectionsModel* model)
{
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    NX_ASSERT(m_models.contains(model));
    m_models.removeOne(model);
}

void QnClientRecentConnectionsManager::updateModelBinding(QnRecentLocalConnectionsModel* model)
{
    NX_ASSERT(model);
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    model->updateData(m_dataCache.value(model->systemId()));
}

void QnClientRecentConnectionsManager::updateModelsData()
{
    NX_ASSERT(!m_updating);
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    m_dataCache.clear();
    const auto lastConnectionsData = qnClientCoreSettings->recentLocalConnections();
    for (const auto connectionDesc : lastConnectionsData)
        m_dataCache[connectionDesc.localId].append(connectionDesc);

    for (const auto model : m_models)
    {
        NX_ASSERT(model);
        if (!model || model->systemId().isNull())
            continue;

        model->updateData(m_dataCache.value(model->systemId()));
    }
}
