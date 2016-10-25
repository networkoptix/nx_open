
#include "system_weights_manager.h"

#include <finders/systems_finder.h>
#include <nx/utils/log/assert.h>
#include <client_core/client_core_settings.h>

QnSystemsWeightsManager::QnSystemsWeightsManager():
    base_type(),
    m_finder(nullptr),
    m_weights()
{}

void QnSystemsWeightsManager::setSystemsFinder(QnAbstractSystemsFinder* finder)
{
    if (m_finder)
        disconnect(m_finder, nullptr, this, nullptr);

    resetWeights();
    if (!finder)
        return;

    m_finder = finder;
    connect(m_finder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &QnSystemsWeightsManager::processSystemDiscovered);

    for (const auto system : m_finder->systems())
        processSystemDiscovered(system);

    // TODO: add processing of systems's removal
}

void QnSystemsWeightsManager::addLocalWeightData(const QnWeightData& data)
{
    const auto localId = data.localId.toString();
    const bool found = m_weights.contains(localId);
    NX_ASSERT(!found, "We don't expect local weight data precence here");

    if (found)
        return;

    m_weights.insert(localId, data);
    emit weightsChanged();

    qnClientCoreSettings->setLocalSystemWeightsData(m_weights.values());

    // TODO: update unknown systemWeight, before review
}

void QnSystemsWeightsManager::processSystemDiscovered(const QnSystemDescriptionPtr& system)
{
    const auto localSystemId = system->localId().toString();
    if (m_weights.contains(localSystemId))
        return; //< Cloud systems always have their own weight data bound to local and cloud Id


    const auto id = system->id();
    const auto itWeightData = m_weights.find(id);
    if ((itWeightData != m_weights.end()) && system->isCloudSystem())
    {
        // Store weight data with in case of future connections
        const auto cloudWeightData = itWeightData.value();
        addLocalWeightData(cloudWeightData);
        return; //< System has some weight
    }

    // Inserts weight for unknown system
    static const bool kFakeConnection = false;
    const QnWeightData unknownSystemWeight = { system->localId(), m_unknownSystemWeight,
        QDateTime::currentMSecsSinceEpoch(), kFakeConnection };
    addLocalWeightData(unknownSystemWeight);
}

void QnSystemsWeightsManager::setWeights(const QnWeightsDataHash& weights)
{
    if (m_weights == weights)
        return;

    m_weights = weights;
    emit weightsChanged();
}

void QnSystemsWeightsManager::resetWeights()
{
    setWeights(QnWeightsDataHash());
}

QnWeightsDataHash QnSystemsWeightsManager::weights() const
{
    return m_weights;
}
