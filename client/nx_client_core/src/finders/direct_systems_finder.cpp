#include "direct_systems_finder.h"

#include <common/common_module.h>

#include <nx/vms/discovery/manager.h>
#include <network/system_helpers.h>
#include <network/local_system_description.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

namespace {

bool isOldServer(const QnModuleInformation& info)
{
    static const auto kMinVersionWithSystem = QnSoftwareVersion(2, 3);
    return (info.version < kMinVersionWithSystem);
}

bool isCloudAddress(const HostAddress& address)
{
    return nx::network::SocketGlobals::addressResolver()
        .isCloudHostName(address.toString());
}

} // namespace

QnDirectSystemsFinder::QnDirectSystemsFinder(QObject *parent)
    : base_type(parent)
{
    const auto moduleManager = commonModule()->moduleDiscoveryManager();
    NX_ASSERT(moduleManager, Q_FUNC_INFO, "Module finder does not exist");
    if (!moduleManager)
        return;

    moduleManager->onSignals(this,
        &QnDirectSystemsFinder::updateServerData,
        &QnDirectSystemsFinder::updateServerData,
        &QnDirectSystemsFinder::removeServer);
}

QnAbstractSystemsFinder::SystemDescriptionList QnDirectSystemsFinder::systems() const
{
    SystemDescriptionList result;
    for (const auto& description : m_systems.values())
        result.append(description.dynamicCast<QnBaseSystemDescription>());

    return result;
}

QnSystemDescriptionPtr QnDirectSystemsFinder::getSystem(const QString &id) const
{
    const auto systemDescriptions = m_systems.values();
    const auto predicate = [id](const QnSystemDescriptionPtr &desc)
    {
        return (desc->id() == id);
    };

    const auto it = std::find_if(systemDescriptions.begin()
        , systemDescriptions.end(), predicate);

    return (it == systemDescriptions.end()
        ? QnSystemDescriptionPtr() : QnSystemDescriptionPtr(*it));
}

void QnDirectSystemsFinder::removeSystem(const SystemsHash::iterator& it)
{
    if (it == m_systems.end())
        return;

    NX_LOGX(lm("Removed system %1").arg(it.value()->id()), cl_logDEBUG2);
    const auto system = it.value();
    for (const auto server: system->servers())
        system->removeServer(server.id);

    m_systems.erase(it);
    emit systemLostInternal(system->id(), system->localId());
    emit systemLost(system->id());
}

void QnDirectSystemsFinder::updateServerData(nx::vms::discovery::ModuleEndpoint module)
{
    const auto systemId = helpers::getTargetSystemId(module);

    const auto systemIt = getSystemItByServer(module.id);
    if (systemIt != m_systems.end())
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
            NX_LOGX(lm("New server %1 updates existing system %2").args(
                module.id, current->id()), cl_logDEBUG2);

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
        const auto systemName = (isOldServer(module)
            ? tr("System")
            : module.systemName);

        const bool isNewSystem = helpers::isNewSystem(module);
        const auto localId = helpers::getLocalSystemId(module);
        const auto systemDescription = (isNewSystem
            ? QnLocalSystemDescription::createFactory(systemId)
            : QnLocalSystemDescription::create(systemId, localId, systemName));

        itSystem = m_systems.insert(systemId, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->containsServer(module.id))
        systemDescription->updateServer(module);
    else
        systemDescription->addServer(module, QnSystemDescription::kDefaultPriority);

    m_serverToSystem[module.id] = systemId;
    if (createNewSystem)
    {
        NX_LOGX(lm("New system %1").arg(systemDescription->id()), cl_logDEBUG2);
        emit systemDiscovered(systemDescription);
    }

    updatePrimaryAddress(module); //< Can remove server and, therefore, whole system.
}

void QnDirectSystemsFinder::removeServer(QnUuid id)
{
    const auto systemIt = getSystemItByServer(id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    NX_LOGX(lm("Removed server %1 from system %2").args(id, systemIt.value()->id()), cl_logDEBUG2);
    const auto removedCount = m_serverToSystem.remove(id);
    if (!removedCount)
        return;

    const auto systemDescription = systemIt.value();
    systemDescription->removeServer(id);
    if (!systemDescription->servers().isEmpty())
        return;

    removeSystem(systemIt);
}

void QnDirectSystemsFinder::updateServerInternal(
    const SystemsHash::iterator systemIt, nx::vms::discovery::ModuleEndpoint module)
{
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    auto systemDescription = systemIt.value();
    const auto changes = systemDescription->updateServer(module);
    if (!changes.testFlag(QnServerField::CloudId))
    {
        updatePrimaryAddress(module);
        return;
    }

    NX_LOGX(lm("Update server %1").arg(module.id), cl_logDEBUG2);

    // Factory status has changed. We have to
    // remove server from current system and add to new
    const auto serverHost = systemDescription->getServerHost(module.id);
    removeServer(module.id);
    updateServerData(module);

    module.endpoint = nx::network::url::getEndpoint(serverHost);
    updatePrimaryAddress(std::move(module));
}

void QnDirectSystemsFinder::updatePrimaryAddress(nx::vms::discovery::ModuleEndpoint module)
{
    auto systemIt = getSystemItByServer(module.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    const bool isCloudHost = isCloudAddress(module.endpoint.address);

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
        .setScheme(module.sslAllowed ? lit("https") : lit("http"))
        .setEndpoint(module.endpoint).toUrl();
    systemDescription->setServerHost(module.id, url);

    // Workaround for systems with version less than 2.3.
    // In these systems only one server is visible, and system name does not exist.

    static const auto kNameTemplate = tr("System (%1)", "%1 is ip and port of system");
    if (isOldServer(module))
        systemDescription->setName(kNameTemplate.arg(module.endpoint.toString()));

}

QnDirectSystemsFinder::SystemsHash::iterator
QnDirectSystemsFinder::getSystemItByServer(const QnUuid &serverId)
{
    const auto it = m_serverToSystem.find(serverId);
    if (it == m_serverToSystem.end())
        return m_systems.end();

    const auto systemId = it.value();
    return m_systems.find(systemId);
}


