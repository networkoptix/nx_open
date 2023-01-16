// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "forgotten_systems_manager.h"

#include <client_core/client_core_settings.h>
#include <finders/systems_finder.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

using nx::vms::client::core::WeightData;
using nx::vms::common::ServerCompatibilityValidator;

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
                    rawSystem = system.data(), servers = system->servers()]()
                {
                    const bool isCompatible = std::any_of(servers.cbegin(), servers.cend(),
                        [](const auto& serverInfo)
                        {
                             return ServerCompatibilityValidator::isCompatibleCustomization(
                                 serverInfo.customization);
                        });

                    if (rawSystem->isOnline() && isCompatible
                        && (isForgotten(id) || isForgotten(localId.toString())))
                    {
                        rememberSystem(id);
                        rememberSystem(localId.toString());
                    }
                };

            connect(system.data(), &QnBaseSystemDescription::onlineStateChanged,
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

    // Do not hide online systems and do not clear its weights.
    if (system && system->isOnline())
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

QStringList QnForgottenSystemsManager::forgottenSystems() const
{
    return m_systems.values();
}

void QnForgottenSystemsManager::rememberSystem(const QString& id)
{
    if (m_systems.remove(id))
        emit forgottenSystemRemoved(id);
}
