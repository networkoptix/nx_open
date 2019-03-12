#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/connection_server/user_locker.h>
#include <nx/network/socket_common.h>
#include <nx/sql/types.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/basic_service_settings.h>
#include <nx/utils/std/optional.h>

#include <nx/clusterdb/engine/p2p_sync_settings.h>

#include "access_control/login_enumeration_protector.h"

namespace nx::cloud::db {
namespace conf {

class Auth
{
public:
    QString rulesXmlPath;
    std::chrono::seconds nonceValidityPeriod;
    std::chrono::seconds intermediateResponseValidityPeriod;
    std::chrono::seconds offlineUserHashValidityPeriod;

    std::chrono::milliseconds checkForExpiredAuthPeriod;
    std::chrono::milliseconds continueUpdatingExpiredAuthPeriod;
    int maxSystemsToUpdateAtATime;

    Auth();
};

class Notification
{
public:
    QString url;
    bool enabled;

    Notification();
};

class AccountManager
{
public:
    std::chrono::seconds accountActivationCodeExpirationTimeout;
    std::chrono::seconds passwordResetCodeExpirationTimeout;
    std::chrono::milliseconds removeExpiredTemporaryCredentialsPeriod;

    AccountManager();
};

class SystemManager
{
public:
    /**
     * Attempt to use system credentials will result in \a credentialsRemovedPermanently
     * result code during this period after system removal.
     */
    std::chrono::seconds reportRemovedSystemPeriod;
    /** System is removed from DB if has not been activated in this period. */
    std::chrono::seconds notActivatedSystemLivePeriod;
    /** Once per this period expired systems are removed from DB. */
    std::chrono::seconds dropExpiredSystemsPeriod;
    bool controlSystemStatusByDb;

    SystemManager();
};

class EventManager
{
public:
    /**
     * If haven't heard anything from mediaserver during this timeout
     * then closing event stream for mediaserver to send another request.
     */
    std::chrono::seconds mediaServerConnectionIdlePeriod;

    EventManager();
};

class ModuleFinder
{
public:
    QString cloudModulesXmlTemplatePath;
    QString newCloudModulesXmlTemplatePath;

    ModuleFinder();
};

class Http
{
public:
    /**
     * Backlog value to pass to tcpServerSocket->listen call.
     */
    int tcpBacklogSize;
    std::chrono::milliseconds connectionInactivityPeriod;
    std::string maintenanceHtdigestPath;

    Http();
};

class VmsGateway
{
public:
    std::string url;
    std::chrono::milliseconds requestTimeout;

    VmsGateway();
};

class Security
{
public:
    std::optional<network::server::UserLockerSettings> loginLockout;
    std::optional<LoginEnumerationProtectionSettings> loginEnumerationProtectionSettings;
};

/**
 * @note Values specified via command-line have priority over conf file (or win32 registry) values.
 */
class Settings:
    public utils::BasicServiceSettings
{
    using base_type = utils::BasicServiceSettings;

public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    /** Loads settings from both command line and conf file (or win32 registry). */
    virtual QString dataDir() const override;
    virtual nx::utils::log::Settings logging() const override;

    /** List of local endpoints to bind to. By default, 0.0.0.0:3346. */
    std::list<network::SocketAddress> endpointsToListen() const;

    const nx::sql::ConnectionOptions& dbConnectionOptions() const;
    const Auth& auth() const;
    const Notification& notification() const;
    const AccountManager& accountManager() const;
    const SystemManager& systemManager() const;
    const EventManager& eventManager() const;
    const clusterdb::engine::SynchronizationSettings& p2pDb() const;
    const QString& changeUser() const;
    const ModuleFinder& moduleFinder() const;
    const Http& http() const;
    const VmsGateway& vmsGateway() const;
    const Security& security() const;

    void setDbConnectionOptions(const nx::sql::ConnectionOptions& options);

private:
    nx::utils::log::Settings m_logging;
    nx::sql::ConnectionOptions m_dbConnectionOptions;
    Auth m_auth;
    Notification m_notification;
    AccountManager m_accountManager;
    SystemManager m_systemManager;
    EventManager m_eventManager;
    clusterdb::engine::SynchronizationSettings m_p2pDb;
    QString m_changeUser;
    ModuleFinder m_moduleFinder;
    Http m_http;
    VmsGateway m_vmsGateway;
    Security m_security;

    virtual void loadSettings() override;

    void loadAuth();

    void loadSecurity();
    void loadLoginLockout();
    void loadHostLockout();
};

} // namespace conf
} // namespace nx::cloud::db
