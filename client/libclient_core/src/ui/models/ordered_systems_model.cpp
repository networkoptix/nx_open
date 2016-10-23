
#include "ordered_systems_model.h"

#include <ui/models/systems_model.h>
#include <nx/utils/string.h>
#include <nx/utils/log/assert.h>
#include <client_core/client_core_settings.h>
#include <helpers/system_weight_helper.h>
#include <utils/common/scoped_value_rollback.h>

QnOrderedSystemsModel::QnOrderedSystemsModel(QObject* parent) :
    base_type(parent),
    m_updateTimer(new QTimer(this)),
    m_cloudWeights(),
    m_localWeights(),
    m_finalWeights(),
    m_maxRealWeight(0.0),
    m_newSystemWeights(),
    m_updatingWeights(false)
{
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::beforeCloudSystemsChanged,
        this, &QnOrderedSystemsModel::handleCloudSystemsChanged);

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
    {
        if (valueId == QnClientCoreSettings::LocalSystemWeightsData)
            handleLocalWeightsChanged();
    });

    handleLocalWeightsChanged();
    handleCloudSystemsChanged(qnCloudStatusWatcher->cloudSystems());

    setSourceModel(new QnSystemsModel(this));

    setDynamicSortFilter(true);
    sort(0);

    static const int kUpdateInterval = 1000 * 60 * 10;// 10 minutes
    m_updateTimer->setInterval(kUpdateInterval);
    connect(m_updateTimer, &QTimer::timeout,
        this, &QnOrderedSystemsModel::updateFinalWeights);
    m_updateTimer->start();
}

bool QnOrderedSystemsModel::getWeightFromData(
    const QModelIndex& modelIndex,
    const IdWeightDataHash& data,
    qreal& weight) const
{
    const auto fillWeightByRole =
        [this, modelIndex, data](int role, qreal& weight)
    {
        const auto id = modelIndex.data(role).toString(); // CHECK BEFOREREVIEW.
        const auto itWeight = data.find(id);
        if (itWeight == data.end())
            return false;

        weight = itWeight.value().weight;
        return true;
    };

    // Searching for maximum weight
    static const QVector<int> kIdRoles =
        { QnSystemsModel::SystemIdRoleId, QnSystemsModel::LocalIdRoleId };

    weight = 0.0;
    bool result = false;
    for (const auto role : kIdRoles)
    {
        qreal foundWeight = 0.0;
        if (fillWeightByRole(role, foundWeight))
        {
            weight = std::max(weight, foundWeight);
            result = true;
        }
    }
    return result;
}

qreal QnOrderedSystemsModel::getWeight(const QModelIndex& modelIndex) const
{
    qreal result = 0.0;
    if (getWeightFromData(modelIndex, m_finalWeights, result)
        || getWeightFromData(modelIndex, m_newSystemWeights, result))
    {
        return result;
    }

    // No weights found: add new system weight
    QN_SCOPED_VALUE_ROLLBACK(&m_updatingWeights, true);
    const auto localId = QnUuid(modelIndex.data(QnSystemsModel::LocalIdRoleId).toUuid());
    const auto newSystemWeight = m_maxRealWeight + 1;
    const auto lastLoginTime = QDateTime::currentMSecsSinceEpoch();

    m_newSystemWeights.insert(localId.toString(),
        QnWeightData({ localId, newSystemWeight, lastLoginTime, false }));

    auto weightsData = qnClientCoreSettings->localSystemWeightsData();
    const auto it = std::find_if(weightsData.begin(), weightsData.end(),
        [localId](const QnWeightData& data) { return data.localId == localId; });

    if (it == weightsData.end())
    {
        const QnWeightData data = { localId, newSystemWeight, lastLoginTime, false };
        weightsData.append(data);
    }
    else
    {
        it->weight = newSystemWeight;
        it->lastConnectedUtcMs = lastLoginTime;
        it->realConnection = false;
    }

    qnClientCoreSettings->setLocalSystemWeightsData(weightsData);
    qnClientCoreSettings->save();
    return newSystemWeight;
}

bool QnOrderedSystemsModel::lessThan(const QModelIndex& left,
    const QModelIndex& right) const
{
    static const auto finalLess =
        [](const QModelIndex& left, const QModelIndex& right) -> bool
        {
            const auto leftsystemName = left.data(QnSystemsModel::SystemNameRoleId).toString();
            const auto rightSystemName = right.data(QnSystemsModel::SystemNameRoleId).toString();
            const auto compareResult = nx::utils::naturalStringCompare(
                leftsystemName, rightSystemName, Qt::CaseInsensitive);
            if (compareResult)
                return (compareResult < 0);

            const auto leftSystemId = left.data(QnSystemsModel::SystemIdRoleId).toString();
            const auto rightSystemId = right.data(QnSystemsModel::SystemIdRoleId).toString();
            return (leftSystemId < rightSystemId);
        };

    const auto leftIsFactory = left.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    const auto rightIsFactory = right.data(QnSystemsModel::IsFactorySystemRoleId).toBool();
    if (leftIsFactory && rightIsFactory)
        return finalLess(left, right);

    if (leftIsFactory || rightIsFactory)
        return leftIsFactory;

    // Sort by weight
    const auto leftWeight = getWeight(left);
    const auto rightWeight = getWeight(right);
    if (leftWeight == rightWeight)
        return finalLess(left, right);

    return (leftWeight > rightWeight);  // System with greater weight will be placed at begin
}

bool QnOrderedSystemsModel::filterAcceptsRow(int row,
    const QModelIndex &parent) const
{
    // Filters out offline non-cloud systems with last connection more than N (defined) days ago
    const auto index = sourceModel()->index(row, 0, parent);
    if (!index.isValid())
        return true;

    if (index.data(QnSystemsModel::IsOnlineRoleId).toBool()
        || index.data(QnSystemsModel::IsCloudSystemRoleId).toBool())
    {
        return true;   //< Skip online or cloud systems
    }

    const auto systemId = index.data(QnSystemsModel::SystemIdRoleId).toString();
    const auto itFinalSystemWeight = m_finalWeights.find(systemId);
    if (itFinalSystemWeight == m_finalWeights.end())
        return true;

    static const auto kMinWeight = 0.00001;
    const auto weight = itFinalSystemWeight.value().weight;
    return (weight > kMinWeight);
}

void QnOrderedSystemsModel::handleCloudSystemsChanged(const QnCloudSystemList& systems)
{
    const auto recentSystems = qnClientCoreSettings->recentCloudSystems();

    static const auto getWeightsData =
        [](const QnCloudSystemList& systems) -> IdWeightDataHash
        {
            IdWeightDataHash result;
            for (const auto system : systems)
            {
                const auto weightData = QnWeightData();

                // Cloud connections are real always
                result.insert(system.cloudId, {system.cloudId, system.weight,
                    system.lastLoginTimeUtcMs, true });
            }
            return result;
        };

    const auto onlineWeights = getWeightsData(systems);
    auto newCloudSystemsData = getWeightsData(recentSystems);
    for (auto it = onlineWeights.begin(); it != onlineWeights.end(); ++it)
        newCloudSystemsData[it.key()] = it.value(); // << Replaces with updated online data

    if (newCloudSystemsData == m_cloudWeights)
        return;

    m_cloudWeights = newCloudSystemsData;

    updateFinalWeights();
}

void QnOrderedSystemsModel::handleLocalWeightsChanged()
{
    if (m_updatingWeights)
        return;

    const auto localWeightData = qnClientCoreSettings->localSystemWeightsData();

    IdWeightDataHash newWeights;
    for (const auto data : localWeightData)
        newWeights.insert(data.localId.toString(), data);

    if (newWeights == m_localWeights)
        return;

    m_localWeights = newWeights;
    m_newSystemWeights.clear();

    updateFinalWeights();
}

void QnOrderedSystemsModel::updateFinalWeights()
{
    if (m_updatingWeights)
        return;

    auto newWeights = m_localWeights;
    for (auto it = m_cloudWeights.begin(); it != m_cloudWeights.end(); ++it)
    {
        const auto nextWeightData = it.value();
        auto& currentWeightData = newWeights[it.key()];

        // Force to replace with greater weight to prevent "jumping"
        if (nextWeightData.weight > currentWeightData.weight)
            currentWeightData = nextWeightData;
    }

    // Recalculates weights according to last connection time
    for (auto& data : newWeights)
        data.weight = helpers::calculateSystemWeight(data.weight, data.lastConnectedUtcMs);

    if (newWeights == m_finalWeights)
        return;


    qDebug() << "/n";
    for (const auto weight : newWeights)
        qDebug() << weight.weight;

    qDebug() << "/n";
    m_finalWeights = newWeights;

    updateMaxRealWeight();

    invalidate();
}

void QnOrderedSystemsModel::updateMaxRealWeight()
{
    m_maxRealWeight = 0.0;
    for (const auto& weightData : m_finalWeights)
    {
        if (weightData.realConnection)
            m_maxRealWeight = std::max(m_maxRealWeight, weightData.weight);
    }
}
