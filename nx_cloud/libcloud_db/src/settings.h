/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_SETTING_H
#define NX_CLOUD_DB_SETTING_H

#include <chrono>
#include <list>
#include <map>

#include <nx/network/socket_common.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/settings.h>

#include <utils/common/command_line_parser.h>
#include <utils/db/types.h>
#include <utils/email/email.h>

#include "ec2/p2p_sync_settings.h"

namespace nx {
namespace cdb {
namespace conf {

class Auth
{
public:
    QString rulesXmlPath;
    std::chrono::seconds nonceValidityPeriod;
    std::chrono::seconds intermediateResponseValidityPeriod;
    std::chrono::milliseconds connectionInactivityPeriod;
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
    std::chrono::seconds passwordResetCodeExpirationTimeout;
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


/**
 * @note Values specified via command-line have priority over conf file (or win32 registry) values.
 */
class Settings
{
public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool showHelp() const;

    /** List of local endpoints to bind to. By default, 0.0.0.0:3346. */
    std::list<SocketAddress> endpointsToListen() const;
    QString dataDir() const;
    
    const nx::utils::log::Settings& logging() const;
    const nx::utils::log::Settings& vmsSynchronizationLogging() const;
    const db::ConnectionOptions& dbConnectionOptions() const;
    const Auth& auth() const;
    const Notification& notification() const;
    const AccountManager& accountManager() const;
    const SystemManager& systemManager() const;
    const EventManager& eventManager() const;
    const ec2::Settings& p2pDb() const;
    const QString& changeUser() const;

    /** Loads settings from both command line and conf file (or win32 registry). */
    void load( int argc, char **argv );
    void printCmdLineArgsHelpToCout();

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelp;

    nx::utils::log::Settings m_logging;
    nx::utils::log::Settings m_vmsSynchronizationLogging;
    db::ConnectionOptions m_dbConnectionOptions;
    Auth m_auth;
    Notification m_notification;
    AccountManager m_accountManager;
    SystemManager m_systemManager;
    EventManager m_eventManager;
    ec2::Settings m_p2pDb;
    QString m_changeUser;

    void fillSupportedCmdParameters();
    void loadConfiguration();
};

} // namespace conf
} // namespace cdb
} // namespace nx

#endif  //NX_CLOUD_DB_SETTING_H
