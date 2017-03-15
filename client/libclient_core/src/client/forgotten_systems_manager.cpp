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
                [this, id = system->id(), localId = system->localId(),
                    rawSystem = system.data()]()
                {
                    if (rawSystem->isConnectable()
                        && (isForgotten(id) || isForgotten(localId.toString())))
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

    connect(this, &QnForgottenSystemsManager::forgottenSystemAdded, this, storeSystems);
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
