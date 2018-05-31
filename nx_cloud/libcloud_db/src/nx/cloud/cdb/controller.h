#pragma once

#include <memory>

#include <nx/network/http/auth_restriction_list.h>
#include <nx/utils/timer_manager.h>

#include <nx/data_sync_engine/synchronization_engine.h>

#include "access_control/authentication_manager.h"
#include "access_control/authorization_manager.h"
#include "dao/rdb/db_instance_controller.h"
#include "ec2/vms_p2p_command_bus.h"
#include "managers/account_manager.h"
#include "managers/authentication_provider.h"
#include "managers/cloud_module_url_provider.h"
#include "managers/email_manager.h"
#include "managers/event_manager.h"
#include "managers/maintenance_manager.h"
#include "managers/system_health_info_provider.h"
#include "managers/system_capabilities_info_provider.h"
#include "managers/system_manager.h"
#include "managers/system_merge_manager.h"
#include "managers/temporary_account_password_manager.h"
#include "managers/vms_gateway.h"
#include "stree/stree_manager.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; }

class Controller
{
public:
    Controller(const conf::Settings& settings);
    ~Controller();

    const StreeManager& streeManager() const;

    TemporaryAccountPasswordManager& tempPasswordManager();

    AccountManager& accountManager();

    EventManager& eventManager();

    data_sync_engine::SyncronizationEngine& ec2SyncronizationEngine();

    AbstractSystemHealthInfoProvider& systemHealthInfoProvider();

    SystemManager& systemManager();

    AbstractSystemMergeManager& systemMergeManager();

    AuthenticationProvider& authProvider();

    MaintenanceManager& maintenanceManager();

    CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated();

    CloudModuleUrlProvider& cloudModuleUrlProvider();

    AuthenticationManager& authenticationManager();

    AuthorizationManager& authorizationManager();

private:
    const conf::Settings& m_settings;
    dao::rdb::DbInstanceController m_dbInstanceController;
    std::unique_ptr<AbstractEmailManager> m_emailManager;
    StreeManager m_streeManager;
    TemporaryAccountPasswordManager m_tempPasswordManager;
    AccountManager m_accountManager;
    EventManager m_eventManager;
    data_sync_engine::SyncronizationEngine m_ec2SyncronizationEngine;
    ec2::VmsP2pCommandBus m_vmsP2pCommandBus;
    std::unique_ptr<AbstractSystemHealthInfoProvider> m_systemHealthInfoProvider;
    nx::utils::StandaloneTimerManager m_timerManager;
    SystemManager m_systemManager;
    SystemCapabilitiesProvider m_systemCapabilitiesProvider;
    VmsGateway m_vmsGateway;
    SystemMergeManager m_systemMergeManager;
    AuthenticationProvider m_authProvider;
    MaintenanceManager m_maintenanceManager;
    CloudModuleUrlProvider m_cloudModuleUrlProviderDeprecated;
    CloudModuleUrlProvider m_cloudModuleUrlProvider;

    std::unique_ptr<nx::network::http::AuthMethodRestrictionList> m_authRestrictionList;
    std::unique_ptr<AuthenticationManager> m_authenticationManager;
    std::unique_ptr<AuthorizationManager> m_authorizationManager;

    void performDataMigrations();
    void generateUserAuthRecords(nx::utils::db::QueryContext* queryContext);

    void initializeDataSynchronizationEngine();
    void initializeSecurity();
};

} // namespace cdb
} // namespace nx
