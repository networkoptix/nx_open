#include "controller.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {

static const QnUuid kCdbGuid("{674bafd7-4eec-4bba-84aa-a1baea7fc6db}");

Controller::Controller(const conf::Settings& settings):
    m_dbInstanceController(settings.dbConnectionOptions()),
    m_emailManager(EMailManagerFactory::create(settings)),
    m_streeManager(settings.auth().rulesXmlPath),
    m_tempPasswordManager(
        settings,
        &m_dbInstanceController.queryExecutor()),
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
        &m_ec2SyncronizationEngine.connectionManager(),
        &m_dbInstanceController.queryExecutor()),
    m_systemManager(
        settings,
        &m_timerManager,
        &m_accountManager,
        m_systemHealthInfoProvider,
        &m_dbInstanceController.queryExecutor(),
        m_emailManager.get(),
        &m_ec2SyncronizationEngine),
    m_authProvider(
        settings,
        &m_accountManager,
        &m_systemManager,
        m_tempPasswordManager,
        &m_vmsP2pCommandBus),
    m_maintenanceManager(
        kCdbGuid,
        &m_ec2SyncronizationEngine,
        m_dbInstanceController)
{
    m_ec2SyncronizationEngine.subscribeToSystemDeletedNotification(
        m_systemManager.systemMarkedAsDeletedSubscription());
    m_timerManager.start();
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

SystemHealthInfoProvider& Controller::systemHealthInfoProvider()
{
    return m_systemHealthInfoProvider;
}

SystemManager& Controller::systemManager()
{
    return m_systemManager;
}

AuthenticationProvider& Controller::authProvider()
{
    return m_authProvider;
}

MaintenanceManager& Controller::maintenanceManager()
{
    return m_maintenanceManager;
}

} // namespace cdb
} // namespace nx
