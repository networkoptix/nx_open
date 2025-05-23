// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "direct_system_finder.h"

#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/discovery/manager.h>

#include "local_system_description.h"
#include "search_address_manager.h"

namespace nx::vms::client::core {

namespace {

bool isOldServer(const nx::vms::api::ModuleInformation& info)
{
    static const nx::utils::SoftwareVersion kMinVersionWithSystem = {2, 3};
    return info.version < kMinVersionWithSystem;
}

bool isCloudAddress(const nx::network::HostAddress& address)
{
    return nx::network::SocketGlobals::addressResolver()
        .isCloudHostname(address.toString());
}

} // namespace

DirectSystemFinder::DirectSystemFinder(
    SearchAddressManager* searchAddressManager,
    QObject* parent)
    :
    base_type(parent),
    m_searchAddressManager(searchAddressManager)
{
    const auto moduleManager = appContext()->moduleDiscoveryManager();
    if (!NX_ASSERT(moduleManager, "Module finder does not exist"))
        return;

    moduleManager->onSignals(this,
        &DirectSystemFinder::updateServerData,
        &DirectSystemFinder::updateServerData,
        &DirectSystemFinder::removeServer);
}

SystemDescriptionList DirectSystemFinder::systems() const
{
    SystemDescriptionList result;
    for (const auto& description : m_systems.values())
        result.append(description.dynamicCast<SystemDescription>());

    return result;
}

SystemDescriptionPtr DirectSystemFinder::getSystem(const QString &id) const
{
    const auto systemDescriptions = m_systems.values();
    const auto predicate = [id](const SystemDescriptionPtr &desc)
    {
        return (desc->id() == id);
    };

    const auto it = std::find_if(
        systemDescriptions.begin(), systemDescriptions.end(), predicate);

    return (it == systemDescriptions.end()
        ? SystemDescriptionPtr() : SystemDescriptionPtr(*it));
}

void DirectSystemFinder::removeSystem(SystemsHash::iterator it)
{
    if (it == m_systems.end())
        return;

    const auto& system = it.value();
    NX_VERBOSE(this, "Removing %1", system);

    for (const auto& server: system->servers())
        system->removeServer(server.id);

    m_systems.erase(it);
    emit systemLost(system->id(), system->localId());
}

void DirectSystemFinder::updateServerData(const nx::vms::discovery::ModuleEndpoint& module)
{
    const nx::Uuid localSystemId = helpers::getLocalSystemId(module);
    const QString systemId = helpers::getTargetSystemId(module);

    NX_VERBOSE(this, "Update server %1 with system id: %2, local id: %3",
        module, systemId, localSystemId);

    m_searchAddressManager->updateServerRemoteAddresses(
        localSystemId,
        module.id,
        module.remoteAddresses
    );

    if (const auto systemIt = getSystemItByServer(module.id); systemIt != m_systems.end())
    {
        /**
        * We can check for system state/id change only here because in this
        * finder system exists only if at least one server is attached
        */
        const auto current = systemIt.value();

        // Checks if "new system" state hasn't been changed yet
        const bool sameNewSystemState =
            (current->isNewSystem() == helpers::isNewSystem(module));

        // Checks if server has same system id (in case of merge/restore operations)
        const bool belongsToSameSystem = (current->id() == systemId);

        if (sameNewSystemState && belongsToSameSystem)
        {
            NX_VERBOSE(this, "Same new system state and belongs to same system");

            // Just update system
            updateServerInternal(systemIt, module);
            return;
        }
        removeServer(module.id);
    }

    auto itSystem = m_systems.find(systemId);
    const auto createNewSystem = (itSystem == m_systems.end());
    if (createNewSystem)
    {
        // Do not allow servers with cloud host to be discovered.
        if (isCloudAddress(module.endpoint.address))
            return;

        NX_VERBOSE(this, "Creating new system id: %1, local id: %2", systemId, localSystemId);

        const auto systemName = (isOldServer(module)
            ? tr("Site")
            : module.systemName);

        const bool isNewSystem = helpers::isNewSystem(module);
        const auto systemDescription = (isNewSystem
            ? LocalSystemDescription::createFactory(systemId)
            : LocalSystemDescription::create(
                systemId,
                localSystemId,
                module.cloudSystemId,
                systemName));

        itSystem = m_systems.insert(systemId, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->containsServer(module.id))
        systemDescription->updateServer(module);
    else
        systemDescription->addServer(module);

    m_serverToSystem[module.id] = systemId;

    updatePrimaryAddress(module); //< Can remove server and, therefore, whole system.

    // Notify of system discovered only if it was not removed on the previous step.
    if (createNewSystem && m_systems.contains(systemId))
    {
        NX_VERBOSE(this, "New system discovered %1", systemDescription);
        emit systemDiscovered(systemDescription);
    }
}

void DirectSystemFinder::removeServer(nx::Uuid id)
{
    const auto systemIt = getSystemItByServer(id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    if (!serverIsInKnownSystem)
    {
        NX_WARNING(this, nx::format("Server %1 is not known").arg(id));
        return;
    }

    const auto& system = systemIt.value();
    NX_VERBOSE(this, "Removing server %1 from %2", id, system);

    if (!m_serverToSystem.remove(id))
        return;

    system->removeServer(id);
    if (system->servers().isEmpty())
        removeSystem(systemIt);
}

void DirectSystemFinder::updateServerInternal(
    SystemsHash::iterator systemIt, const nx::vms::discovery::ModuleEndpoint& module)
{
    if (!NX_ASSERT(systemIt != m_systems.end(), "Server %1 is not in known system", module.id))
        return;

    auto systemDescription = systemIt.value();
    NX_VERBOSE(this, "Update server %1 for %2", module, systemDescription);

    const auto changes = systemDescription->updateServer(module);
    if (!changes.testFlag(QnServerField::CloudId))
    {
        updatePrimaryAddress(module);
        return;
    }

    // Factory status has changed. We have to
    // remove server from current system and add to new
    const auto serverHost = systemDescription->getServerHost(module.id);
    removeServer(module.id);
    updateServerData(module);

    auto updatedModule = module;
    updatedModule.endpoint = nx::network::url::getEndpoint(serverHost);
    NX_ASSERT(!updatedModule.endpoint.address.isEmpty());
    updatePrimaryAddress(updatedModule);
}

void DirectSystemFinder::updatePrimaryAddress(const nx::vms::discovery::ModuleEndpoint& module)
{
    auto systemIt = getSystemItByServer(module.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    const bool isCloudHost = isCloudAddress(module.endpoint.address);

    NX_VERBOSE(this, "Update primary address, server %1, known system: %2",
        module, serverIsInKnownSystem);

    if (isCloudHost)
    {
        // Do not allow servers with cloud host to be discovered
        if (serverIsInKnownSystem)
            removeServer(module.id);

        return;
    }
    else if (!serverIsInKnownSystem)
    {
        // Primary address was changed from cloud to direct
        updateServerData(module);
        systemIt = getSystemItByServer(module.id);
        if (systemIt == m_systems.end())
            return;
    }


    const auto systemDescription = systemIt.value();
    const auto url = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(module.endpoint).toUrl();
    systemDescription->setServerHost(module.id, url);

    // Workaround for systems with version less than 2.3.
    // In these systems only one server is visible, and system name does not exist.

    static const auto kNameTemplate = tr("Site (%1)", "%1 is ip and port of the site");
    if (isOldServer(module))
        systemDescription->setName(nx::format(kNameTemplate).arg(module.endpoint.toString()));
}

DirectSystemFinder::SystemsHash::iterator
DirectSystemFinder::getSystemItByServer(const nx::Uuid &serverId)
{
    const auto it = m_serverToSystem.find(serverId);
    if (it == m_serverToSystem.end())
        return m_systems.end();

    const auto systemId = it.value();
    return m_systems.find(systemId);
}

} // namespace nx::vms::client::core
