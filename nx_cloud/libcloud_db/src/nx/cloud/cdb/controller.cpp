#include "controller.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>

#include "http_handlers/ping.h"
#include "settings.h"

namespace nx {
namespace cdb {

static const QnUuid kCdbGuid("{674bafd7-4eec-4bba-84aa-a1baea7fc6db}");

Controller::Controller(const conf::Settings& settings):
    m_dbInstanceController(settings.dbConnectionOptions()),
    m_emailManager(EMailManagerFactory::create(settings)),
    m_streeManager(settings.auth().rulesXmlPath),
    m_tempPasswordManager(&m_dbInstanceController.queryExecutor()),
    m_accountManager(
        settings,
        m_streeManager,
        &m_tempPasswordManager,
        &m_dbInstanceController.queryExecutor(),
        m_emailManager.get()),
    m_eventManager(settings),
    m_ec2SyncronizationEngine(
        kCdbGuid,
        settings.p2pDb(),
        &m_dbInstanceController.queryExecutor()),
    m_vmsP2pCommandBus(&m_ec2SyncronizationEngine),
    m_systemHealthInfoProvider(
        SystemHealthInfoProviderFactory::instance().create(
            &m_ec2SyncronizationEngine.connectionManager(),
            &m_dbInstanceController.queryExecutor())),
    m_systemManager(
        settings,
        &m_timerManager,
        &m_accountManager,
        *m_systemHealthInfoProvider,
        &m_dbInstanceController.queryExecutor(),
        m_emailManager.get(),
        &m_ec2SyncronizationEngine),
    m_vmsGateway(settings, m_accountManager),
    m_systemMergeManager(
        &m_systemManager,
        *m_systemHealthInfoProvider,
        &m_vmsGateway,
        &m_dbInstanceController.queryExecutor()),
    m_authProvider(
        settings,
        &m_dbInstanceController.queryExecutor(),
        &m_accountManager,
        &m_systemManager,
        m_tempPasswordManager,
        &m_vmsP2pCommandBus),
    m_maintenanceManager(
        kCdbGuid,
        &m_ec2SyncronizationEngine,
        m_dbInstanceController),
    m_cloudModuleUrlProviderDeprecated(
        settings.moduleFinder().cloudModulesXmlTemplatePath),
    m_cloudModuleUrlProvider(
        settings.moduleFinder().newCloudModulesXmlTemplatePath)
{
    performDataMigrations();

    initializeDataSynchronizationEngine();

    m_timerManager.start();

    initializeSecurity();
}

Controller::~Controller()
{
    m_ec2SyncronizationEngine.unsubscribeFromSystemDeletedNotification(
        m_systemManager.systemMarkedAsDeletedSubscription());
}

const StreeManager& Controller::streeManager() const
{
    return m_streeManager;
}

TemporaryAccountPasswordManager& Controller::tempPasswordManager()
{
    return m_tempPasswordManager;
}

AccountManager& Controller::accountManager()
{
    return m_accountManager;
}

EventManager& Controller::eventManager()
{
    return m_eventManager;
}

ec2::SyncronizationEngine& Controller::ec2SyncronizationEngine()
{
    return m_ec2SyncronizationEngine;
}

AbstractSystemHealthInfoProvider& Controller::systemHealthInfoProvider()
{
    return *m_systemHealthInfoProvider;
}

SystemManager& Controller::systemManager()
{
    return m_systemManager;
}

AbstractSystemMergeManager& Controller::systemMergeManager()
{
    return m_systemMergeManager;
}

AuthenticationProvider& Controller::authProvider()
{
    return m_authProvider;
}

MaintenanceManager& Controller::maintenanceManager()
{
    return m_maintenanceManager;
}

CloudModuleUrlProvider& Controller::cloudModuleUrlProviderDeprecated()
{
    return m_cloudModuleUrlProviderDeprecated;
}

CloudModuleUrlProvider& Controller::cloudModuleUrlProvider()
{
    return m_cloudModuleUrlProvider;
}

AuthenticationManager& Controller::authenticationManager()
{
    return *m_authenticationManager;
}

AuthorizationManager& Controller::authorizationManager()
{
    return *m_authorizationManager;
}

void Controller::performDataMigrations()
{
    using namespace std::placeholders;

    NX_INFO(this, "Performing data migrations...");

    try
    {
        if (m_dbInstanceController.isUserAuthRecordsMigrationNeeded())
        {
            m_dbInstanceController.queryExecutor().executeUpdateQuerySync(
                std::bind(&Controller::generateUserAuthRecords, this, _1));
        }
    }
    catch (const std::exception e)
    {
        NX_ERROR(this, lm("Failed to do data migrations. %1").arg(e.what()));
    }
}

void Controller::generateUserAuthRecords(nx::utils::db::QueryContext* queryContext)
{
    const auto allSystemSharings = m_systemManager.fetchAllSharings();
    for (const auto& sharing: allSystemSharings)
    {
        m_authProvider.afterSharingSystem(
            queryContext,
            sharing,
            nx::cdb::SharingType::sharingWithExistingAccount);
    }
}

void Controller::initializeSecurity()
{
    // TODO: #ak Move following to stree xml.
    m_authRestrictionList = std::make_unique<nx_http::AuthMethodRestrictionList>();
    m_authRestrictionList->allow(kDeprecatedCloudModuleXmlPath, nx_http::AuthMethod::noAuth);
    m_authRestrictionList->allow(kDiscoveryCloudModuleXmlPath, nx_http::AuthMethod::noAuth);
    m_authRestrictionList->allow(http_handler::Ping::kHandlerPath, nx_http::AuthMethod::noAuth);
    m_authRestrictionList->allow(kAccountRegisterPath, nx_http::AuthMethod::noAuth);
    m_authRestrictionList->allow(kAccountActivatePath, nx_http::AuthMethod::noAuth);
    m_authRestrictionList->allow(kAccountReactivatePath, nx_http::AuthMethod::noAuth);

    std::vector<AbstractAuthenticationDataProvider*> authDataProviders;
    authDataProviders.push_back(&m_accountManager);
    authDataProviders.push_back(&m_systemManager);
    m_authenticationManager = std::make_unique<AuthenticationManager>(
        std::move(authDataProviders),
        *m_authRestrictionList,
        m_streeManager);

    m_authorizationManager = std::make_unique<AuthorizationManager>(
        m_streeManager,
        m_accountManager,
        m_systemManager,
        m_systemManager);
}

void Controller::initializeDataSynchronizationEngine()
{
    m_ec2SyncronizationEngine.subscribeToSystemDeletedNotification(
        m_systemManager.systemMarkedAsDeletedSubscription());

    m_ec2SyncronizationEngine.incomingTransactionDispatcher().registerTransactionHandler
        <::ec2::ApiCommand::saveSystemMergeHistoryRecord, ::ec2::ApiSystemMergeHistoryRecord, int>(
            [this](
                nx::utils::db::QueryContext* queryContext,
                const nx::String& /*systemId*/,
                ::ec2::QnTransaction<::ec2::ApiSystemMergeHistoryRecord> data,
                int*)
            {
                m_systemMergeManager.processMergeHistoryRecord(queryContext, data.params);
                return nx::utils::db::DBResult::ok;
            },
            [](nx::utils::db::QueryContext*, nx::utils::db::DBResult, int) {});
}

} // namespace cdb
} // namespace nx
