
#include "direct_systems_finder.h"

#include <network/module_finder.h>
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
    const auto moduleFinder = qnModuleFinder;
    NX_ASSERT(moduleFinder, Q_FUNC_INFO, "Module finder does not exist");
    if (!moduleFinder)
        return;

    connect(moduleFinder, &QnModuleFinder::moduleChanged
        , this, &QnDirectSystemsFinder::addServer);
    connect(moduleFinder, &QnModuleFinder::moduleLost
        , this, &QnDirectSystemsFinder::removeServer);
    connect(moduleFinder, &QnModuleFinder::modulePrimaryAddressChanged
        , this, &QnDirectSystemsFinder::updatePrimaryAddress);

    for (const auto moduleInformation: moduleFinder->discoveredServers())
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

void QnDirectSystemsFinder::addServer(QnModuleInformation moduleInformation)
{
    const auto systemId = helpers::getTargetSystemId(moduleInformation);

    const auto systemIt = getSystemItByServer(moduleInformation.id);
    if (systemIt != m_systems.end())
    {
        /**
        * We can check for system state/id change only here because in this
        * finder system exists only if at least one server is attached
        */
        const auto current = systemIt.value();

        // Checks if "new system" state hasn't been changed yet
        const bool sameNewSystemState =
            (current->isNewSystem() == helpers::isNewSystem(moduleInformation));

        // Checks if server has same system id (in case of merge/restore operations)
        const bool belongsToSameSystem = (current->id() == systemId);

        if (sameNewSystemState && belongsToSameSystem)
        {
            // Just update system
            updateServer(systemIt, moduleInformation);
            return;
        }

        removeServer(moduleInformation);
    }

    auto itSystem = m_systems.find(systemId);
    const auto createNewSystem = (itSystem == m_systems.end());
    if (createNewSystem)
    {
        const auto systemName = (isOldServer(moduleInformation)
            ? tr("System")
            : moduleInformation.systemName);

        const bool isNewSystem = helpers::isNewSystem(moduleInformation);
        const auto localId = helpers::getLocalSystemId(moduleInformation);
        const auto systemDescription = (isNewSystem
            ? QnLocalSystemDescription::createFactory(systemId)
            : QnLocalSystemDescription::create(systemId, localId, systemName));

        itSystem = m_systems.insert(systemId, systemDescription);
    }

    const auto systemDescription = itSystem.value();
    if (systemDescription->containsServer(moduleInformation.id))
        systemDescription->updateServer(moduleInformation);
    else
        systemDescription->addServer(moduleInformation, QnSystemDescription::kDefaultPriority);

    m_serverToSystem[moduleInformation.id] = systemId;

    const auto host = qnModuleFinder->primaryAddress(moduleInformation.id);
    updatePrimaryAddress(moduleInformation, host);

    if (createNewSystem)
        emit systemDiscovered(systemDescription);
}

void QnDirectSystemsFinder::removeServer(const QnModuleInformation &moduleInformation)
{
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const auto serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto removedCount = m_serverToSystem.remove(moduleInformation.id);
    if (!removedCount)
        return;

    const auto systemDescription = systemIt.value();
    systemDescription->removeServer(moduleInformation.id);
    if (!systemDescription->servers().isEmpty())
        return;

    removeSystem(systemIt);
}

void QnDirectSystemsFinder::updateServer(const SystemsHash::iterator systemIt
    , const QnModuleInformation &moduleInformation)
{
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    auto systemDescription = systemIt.value();
    const auto changes = systemDescription->updateServer(moduleInformation);
    if (!changes.testFlag(QnServerField::CloudId))
        return;

    // Factory status has changed. We have to
    // remove server from current system and add to new
    const auto serverHost = systemDescription->getServerHost(moduleInformation.id);
    removeServer(moduleInformation);
    addServer(moduleInformation);
    updatePrimaryAddress(moduleInformation, nx::network::url::getEndpoint(serverHost));
}

void QnDirectSystemsFinder::updatePrimaryAddress(const QnModuleInformation &moduleInformation
    , const SocketAddress &address)
{
    auto systemIt = getSystemItByServer(moduleInformation.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    const bool isCloudHost = isCloudAddress(address.address);

    if (isCloudHost)
    {
        // Do not allow servers with cloud host to be discovered
        if (serverIsInKnownSystem)
            removeServer(moduleInformation);

        return;
    }
    else if (!serverIsInKnownSystem)
    {
        // Primary address was changed from cloud to direct
        addServer(moduleInformation);
        systemIt = getSystemItByServer(moduleInformation.id);
        if (systemIt == m_systems.end())
            return;
    }


    const auto systemDescription = systemIt.value();
    const auto url = nx::network::url::Builder()
        .setScheme(moduleInformation.sslAllowed ? lit("https") : QString())
        .setEndpoint(address).toUrl();
    systemDescription->setServerHost(moduleInformation.id, url);

    // Workaround for systems with version less than 2.3.
    // In these systems only one server is visible, and system name does not exist.

    static const auto kNameTemplate = tr("System (%1)", "%1 is ip and port of system");
    if (isOldServer(moduleInformation))
        systemDescription->setName(kNameTemplate.arg(address.toString()));

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


