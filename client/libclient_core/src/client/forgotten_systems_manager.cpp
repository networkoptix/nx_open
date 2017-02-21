#include "forgotten_systems_manager.h"

#include <nx/utils/log/assert.h>
#include <finders/systems_finder.h>
#include <client_core/client_core_settings.h>

using nx::client::core::WeightData;

QnForgottenSystemsManager::QnForgottenSystemsManager():
    base_type(),
    m_systems()
{
    NX_ASSERT(qnSystemsFinder, "Systems finder is not initialized yet");
    NX_ASSERT(qnClientCoreSettings, "Client core settings is not initialized yet");
    if (!qnSystemsFinder || !qnClientCoreSettings)
        return;

    m_systems = qnClientCoreSettings->forgottenSystems();

    const auto processSystemDiscovered =
        [this](const QnSystemDescriptionPtr& system)
        {
            const auto checkOnlineSystem =
                [this, id = system->id(), localId = system->localId(), rawSystem = system.data()]()
                {
                    if (rawSystem->isConnectable())
                    {
                        rememberSystem(id);
                        rememberSystem(localId.toString());
                    }
                };

            connect(system.data(), &QnBaseSystemDescription::connectableStateChanged,
                this, checkOnlineSystem);
            checkOnlineSystem();
        };

    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, processSystemDiscovered);

    for (const auto& system: qnSystemsFinder->systems())
        processSystemDiscovered(system);

    const auto storeSystems =
        [this]()
        {
            qnClientCoreSettings->setForgottenSystems(m_systems);
            qnClientCoreSettings->save();
        };

    const auto resetSystemWeight =
        [](const QString& id)
        {
            const auto localId = QnUuid::fromStringSafe(id);
            if (localId.isNull())
                return;

            auto localWeights = qnClientCoreSettings->localSystemWeightsData();
            const auto itWeight = std::find_if(localWeights.begin(), localWeights.end(),
                [localId](const WeightData& data) { return (localId == data.localId); });

            if (itWeight == localWeights.end())
                return;

            auto& weightData = *itWeight;
            weightData.lastConnectedUtcMs = QDateTime::currentMSecsSinceEpoch();
            weightData.realConnection = true;
            weightData.weight = 0;  //< Resets position to the last

            qnClientCoreSettings->setLocalSystemWeightsData(localWeights);
            // Caller is responsible to call QnClientCoreSettings::save()
        };

    const auto handleSystemForgotten =
        [resetSystemWeight, storeSystems](const QString& id)
        {
            resetSystemWeight(id);
            storeSystems();
        };

    connect(this, &QnForgottenSystemsManager::forgottenSystemAdded, this, handleSystemForgotten);
    connect(this, &QnForgottenSystemsManager::forgottenSystemRemoved, this, storeSystems);
}

void QnForgottenSystemsManager::forgetSystem(const QString& id)
{
    const auto system = qnSystemsFinder->getSystem(id);

    // Do not hide online reachable systems and do not clear its weights
    if (system && system->isConnectable())
        return;

    const bool contains = m_systems.contains(id);
    if (contains)
        return;

    m_systems.insert(id);
    emit forgottenSystemAdded(id);
}

bool QnForgottenSystemsManager::isForgotten(const QString& id) const
{
    return m_systems.contains(id);
}

void QnForgottenSystemsManager::rememberSystem(const QString& id)
{
    if (m_systems.remove(id))
        emit forgottenSystemRemoved(id);
}
