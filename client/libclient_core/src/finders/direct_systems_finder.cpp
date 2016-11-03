
#include "direct_systems_finder.h"

#include <network/module_finder.h>
#include <helpers/system_helpers.h>
#include <nx/network/socket_common.h>

namespace {

bool isOldServer(const QnModuleInformation& info)
{
    static const auto kMinVersionWithSystem = QnSoftwareVersion(2, 3);
    return (info.version < kMinVersionWithSystem);
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
    emit systemLost(system->id());
}

void QnDirectSystemsFinder::addServer(QnModuleInformation moduleInformation)
{
    bool checkForSystemRemoval = true;
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    if (systemIt != m_systems.end())
    {
        // checks if it is new system
        const bool wasNewSystem = systemIt.value()->isNewSystem();
        const bool isNewSystem = helpers::isNewSystem(moduleInformation);

        /**
         * We can check for system state change only here because in this
         * finder system exists only if at least one server is attached
         */
        if (wasNewSystem == isNewSystem)
        {
            // "New system" state is not changed, just update system
            updateServer(systemIt, moduleInformation);
            return;
        }
        else
        {
            /**
             * "New system" state is changed - remove old system in
             * order to create a new one with updated state. Id of system will be changed.
             */
            removeSystem(systemIt);
        }
    }

    const auto systemId = helpers::getTargetSystemId(moduleInformation);
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
            ? QnSystemDescription::createFactorySystem(systemId)
            : QnSystemDescription::createLocalSystem(systemId, localId, systemName));

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
    if (!changes.testFlag(QnServerField::SystemName)
        && !changes.testFlag(QnServerField::CloudId))
    {
        return;
    }

    // System name or factory status has changed. We have to
    // remove server from current system and add to new
    const auto serverHost = systemDescription->getServerHost(moduleInformation.id);
    removeServer(moduleInformation);
    addServer(moduleInformation);
    updatePrimaryAddress(moduleInformation, SocketAddress(serverHost));
}

void QnDirectSystemsFinder::updatePrimaryAddress(const QnModuleInformation &moduleInformation
    , const SocketAddress &address)
{
    const auto systemIt = getSystemItByServer(moduleInformation.id);
    const bool serverIsInKnownSystem = (systemIt != m_systems.end());
    //NX_ASSERT(serverIsInKnownSystem, Q_FUNC_INFO, "Server is not known");
    if (!serverIsInKnownSystem)
        return;

    const auto systemDescription = systemIt.value();
    const auto url = address.toUrl(moduleInformation.sslAllowed ? lit("https") : QString());
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


