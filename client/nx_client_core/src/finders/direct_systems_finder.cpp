#include "direct_systems_finder.h"

#include <common/common_module.h>

#include <nx/vms/discovery/manager.h>
#include <network/system_helpers.h>
#include <network/local_system_description.h>

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
        &QnDirectSystemsFinder::addServer,
        &QnDirectSystemsFinder::updatePrimaryAddress,
        &QnDirectSystemsFinder::removeServer);

    for (const auto moduleInformation: moduleManager->getAll())
        addServer(moduleInformation);
}

QnDirectSystemsFinder::~QnDirectSystemsFinder()
{}

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

    const auto system = it.value();
    for (const auto server: system->servers())
        system->removeServer(server.id);

    m_systems.erase(it);
    emit systemLostInternal(system->id(), system->localId());
    emit systemLost(system->id());
}

void QnDirectSystemsFinder::addServer(nx::vms::discovery::Manager::ModuleData data)
{
    const auto systemId = helpers::getTargetSystemId(data);

    const auto systemIt = getSystemItByServer(data.id);
    if (systemIt != m_systems.end())
    {
        /**
        * We can check for system state/id change only here because in this
        * finder system exists only if at least one server is attached
        */
        const auto current = systemIt.value();

        // Checks if "new system" state hasn't been changed yet
        const bool sameNewSystemState =
            (current->isNewSystem() == helpers::isNewSystem(data));

        // Checks if server has same system id (in case of merge/restore operations)
        const bool belongsToSameSystem = (current->id() == systemId);

        if (sameNewSystemState && belongsToSameSystem)
        {
            // Just update system
            updateServer(systemIt, data);
            return;
        }

        removeServer(data.id);
    }

    auto itSystem = m_systems.find(systemId);
    const auto createNewSystem = (itSystem == m_systems.end());
    if (createNewSystem)
    {
        const auto systemName = (isOldServer(data)
            ? tr("System")
            : data.systemName);

        const bool isNewSystem = helpers::isNewSystem(data);
        const auto localId = helpers::getLocalSystemId(data);
        const auto systemDescription = (isNewSystem
            ? QnLocalSystemDescription::createFactory(systemId)
            : QnLocalSystemDescription::create(systemId, localId, systemName));

        itSystem = m_systems.insert(systemId, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->containsServer(data.id))
        systemDescription->updateServer(data);
    else
        systemDescription->addServer(data, QnSystemDescription::kDefaultPriority);

    m_serverToSystem[data.id] = systemId;
    updatePrimaryAddress(data);

    if (createNewSystem)
        emit systemDiscovered(systemDescription);
}

void QnDirectSystemsFinder::removeServer(QnUuid id)
{
    const auto systemIt = getSystemItByServer(id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto removedCount = m_serverToSystem.remove(id);
    if (!removedCount)
        return;

    const auto systemDescription = systemIt.value();
    systemDescription->removeServer(id);
    if (!systemDescription->servers().isEmpty())
        return;

    removeSystem(systemIt);
}

void QnDirectSystemsFinder::updateServer(
    const SystemsHash::iterator systemIt, nx::vms::discovery::Manager::ModuleData module)
{
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    auto systemDescription = systemIt.value();
    const auto changes = systemDescription->updateServer(module);
    if (!changes.testFlag(QnServerField::CloudId))
        return;

    // Factory status has changed. We have to
    // remove server from current system and add to new
    const auto serverHost = systemDescription->getServerHost(module.id);
    removeServer(module.id);
    addServer(module);

    module.endpoint = nx::network::url::getEndpoint(serverHost);
    updatePrimaryAddress(std::move(module));
}

void QnDirectSystemsFinder::updatePrimaryAddress(nx::vms::discovery::Manager::ModuleData module)
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
        addServer(module);
        systemIt = getSystemItByServer(module.id);
        if (systemIt == m_systems.end())
            return;
    }


    const auto systemDescription = systemIt.value();
    const auto url = nx::network::url::Builder()
        .setScheme(module.sslAllowed ? lit("https") : QString())
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


