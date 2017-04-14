#include "system_weights_manager.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>

#include <finders/systems_finder.h>
#include <watchers/cloud_status_watcher.h>
#include <client_core/client_core_settings.h>
#include <network/cloud_system_data.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <helpers/system_weight_helper.h>

namespace {

void addOrUpdateWeightData(
    const QString& id,
    const WeightData& data,
    QnWeightsDataHash& target)
{
    const auto it = target.find(id);
    if (it == target.end())
        target.insert(id, data);
    else if (data.weight > it.value().weight)
        *it = data;
}

QnWeightsDataHash filterWeights(const QnWeightsDataHash& data)
{
    QnWeightsDataHash result;
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        const auto systemId = it.key();
        const auto weightData = it.value();
        const auto localId = weightData.localId.toString();
        if (systemId == localId) //< It is weight for local system - store it
            addOrUpdateWeightData(localId, weightData, result);
    }
    return result;
}

QnWeightsDataHash fromCloudSystemData(const QnCloudSystemList& data)
{
    QnWeightsDataHash result;
    for (const auto& cloudData: data)
    {
        const WeightData weightData({ cloudData.localId, cloudData.weight,
            cloudData.lastLoginTimeUtcMs, true});

        result.insert(cloudData.cloudId, weightData);
        addOrUpdateWeightData(cloudData.localId.toString(), weightData, result);
    }
    return result;
}

QnWeightsDataHash recalculatedWeights(QnWeightsDataHash source)
{
    using namespace nx::client::core;
    for (auto& data: source)
        data.weight = helpers::calculateSystemWeight(data.weight, data.lastConnectedUtcMs);

    return source;
}

}   // namespace

QnSystemsWeightsManager::QnSystemsWeightsManager():
    base_type(),
    m_updateTimer(new QTimer(this)),
    m_finder(nullptr),
    m_baseWeights(),
    m_updatedWeights(),
    m_unknownSystemWeight(0.0)
{
    NX_ASSERT(qnClientCoreSettings, "Client client settings are not ready");
    NX_ASSERT(qnCloudStatusWatcher, "Cloud status watcher is not ready");

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged,
        this, &QnSystemsWeightsManager::handleSourceWeightsChanged);

    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int value)
        {
            if (value == QnClientCoreSettings::LocalSystemWeightsData)
                handleSourceWeightsChanged();
        });

    setSystemsFinder(qnSystemsFinder);

    static const int kUpdateInterval = 1000 * 60 * 10; //< 10 minutes
    m_updateTimer->setInterval(kUpdateInterval);
    connect(m_updateTimer, &QTimer::timeout,
        this, &QnSystemsWeightsManager::afterBaseWeightsUpdated);

}

void QnSystemsWeightsManager::handleSourceWeightsChanged()
{
    const auto localWeights = qnClientCoreSettings->localSystemWeightsData();

    const auto cloudSystemsData = qnCloudStatusWatcher->cloudSystems();
    auto targetWeights = fromCloudSystemData(cloudSystemsData);

    for (const auto& weightData: localWeights)
        addOrUpdateWeightData(weightData.localId.toString(), weightData, targetWeights);

    if (targetWeights == m_baseWeights)
        return;

    m_baseWeights = targetWeights;
    afterBaseWeightsUpdated();
}

void QnSystemsWeightsManager::setSystemsFinder(QnAbstractSystemsFinder* finder)
{
    if (m_finder)
        disconnect(m_finder, nullptr, this, nullptr);

    resetWeights();

    NX_ASSERT(finder, "System finder is nullptr");
    if (!finder)
        return;

    m_finder = finder;
    connect(m_finder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &QnSystemsWeightsManager::processSystemDiscovered);

    for (const auto& system: m_finder->systems())
        processSystemDiscovered(system);

    // TODO: #ynikitenkov add processing of systems's removal.
}

void QnSystemsWeightsManager::addLocalWeightData(const nx::client::core::WeightData& data)
{
    const auto localId = data.localId.toString();
    const bool found = m_baseWeights.contains(localId);
    NX_ASSERT(!found, "We don't expect local weight data precence here");

    if (found)
        return;

    m_baseWeights.insert(localId, data);
    afterBaseWeightsUpdated();

    qnClientCoreSettings->setLocalSystemWeightsData(
        filterWeights(m_baseWeights).values());
}

void QnSystemsWeightsManager::processSystemDiscovered(const QnSystemDescriptionPtr& system)
{
    const auto localSystemId = system->localId().toString();
    if (m_baseWeights.contains(localSystemId))
        return; //< Cloud systems always have their own weight data bound to local and cloud Id


    const auto id = system->id();
    const auto itWeightData = m_baseWeights.find(id);
    if ((itWeightData != m_baseWeights.end()) && system->isCloudSystem())
    {
        // Store weight data with in case of future connections
        const auto cloudWeightData = itWeightData.value();
        addLocalWeightData(cloudWeightData);
        return;
    }

    // Inserts weight for unknown system
    static const bool kFakeConnection = false;
    const WeightData unknownSystemWeight({ system->localId(), unknownSystemsWeight(),
        QDateTime::currentMSecsSinceEpoch(), kFakeConnection });
    addLocalWeightData(unknownSystemWeight);
}

void QnSystemsWeightsManager::resetWeights()
{
    // Resets weights to stored in cloud and settings
    handleSourceWeightsChanged();
}

QnWeightsDataHash QnSystemsWeightsManager::weights() const
{
    return m_updatedWeights;
}

void QnSystemsWeightsManager::afterBaseWeightsUpdated()
{
    const auto updatedWeights = recalculatedWeights(m_baseWeights);
    if (m_updatedWeights == updatedWeights)
        return;

    m_updatedWeights = updatedWeights;

    qreal targetUnknownSystemWeight = 0;
    for (const auto& data: m_updatedWeights)
    {
        if (data.realConnection)
            targetUnknownSystemWeight = std::max(targetUnknownSystemWeight, data.weight);
    }

    setUnknownSystemsWeight(targetUnknownSystemWeight);

    emit weightsChanged();
}

qreal QnSystemsWeightsManager::unknownSystemsWeight() const
{
    static const qreal kMinimalDiffrence = 0.0000001;
    return std::max<qreal>(m_unknownSystemWeight - kMinimalDiffrence, 0);
}

void QnSystemsWeightsManager::setUnknownSystemsWeight(qreal value)
{
    if (qFuzzyEquals(m_unknownSystemWeight, value))
        return;

    m_unknownSystemWeight = value;
    emit unknownSystemsWeightChanged();
}

void QnSystemsWeightsManager::setWeight(
    const QnUuid& localSystemId,
    qreal weight)
{
    auto localWeights = qnClientCoreSettings->localSystemWeightsData();
    const auto it = std::find_if(localWeights.begin(), localWeights.end(),
        [localSystemId](const nx::client::core::WeightData& data)
        {
            return (data.localId == localSystemId);
        });

    const auto lastConnected = QDateTime::currentMSecsSinceEpoch();
    if (it == localWeights.end())
    {
        localWeights.append(nx::client::core::WeightData({
            localSystemId, weight, lastConnected, true}));
    }
    else
    {
        it->lastConnectedUtcMs = lastConnected;
        it->weight = weight;
    }

    qnClientCoreSettings->setLocalSystemWeightsData(localWeights);
}

