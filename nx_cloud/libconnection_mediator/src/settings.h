#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/settings.h>

#include <utils/common/command_line_parser.h>
#include <utils/db/types.h>
#include <utils/email/email.h>

namespace nx {
namespace hpm {
namespace conf {

struct General
{
    QString systemUserToRunUnder;
    QString dataDir;
};

struct CloudDB
{
    bool runWithCloud;
    QString endpoint;
    QString user;
    QString password;
    std::chrono::seconds updateInterval;

    CloudDB():
        runWithCloud(true)
    {
    }
};

struct Stun
{
    std::list<SocketAddress> addrToListenList;
    boost::optional<KeepAliveOptions> keepAliveOptions;
};

struct Http
{
    std::list<SocketAddress> addrToListenList;
    boost::optional<KeepAliveOptions> keepAliveOptions;
};

struct Statistics
{
    bool enabled;

    Statistics():
        enabled(true)
    {
    }
};

/**
 * @note Values specified via command-line have priority over conf file (or win32 registry) values.
 */
class Settings
{
public:
    Settings();

    bool showHelp() const;

    const General& general() const;
    const nx::utils::log::Settings& logging() const;
    const CloudDB& cloudDB() const;
    const Stun& stun() const;
    const Http& http() const;
    const api::ConnectionParameters& connectionParameters() const;
    const nx::db::ConnectionOptions& dbConnectionOptions() const;
    const Statistics& statistics() const;

    /**
     * Loads settings from both command line and conf file (or win32 registry).
     */
    void load(int argc, char **argv);
    void printCmdLineArgsHelp();

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelp;

    General m_general;
    nx::utils::log::Settings m_logging;
    CloudDB m_cloudDB;
    Stun m_stun;
    Http m_http;
    api::ConnectionParameters m_connectionParameters;
    nx::db::ConnectionOptions m_dbConnectionOptions;
    Statistics m_statistics;

    void fillSupportedCmdParameters();
    void initializeWithDefaultValues();
    void loadConfiguration();
    void readEndpointList(
        const QString& str,
        std::list<SocketAddress>* const addrToListenList);
};

} // namespace conf
} // namespace hpm
} // namespace nx
