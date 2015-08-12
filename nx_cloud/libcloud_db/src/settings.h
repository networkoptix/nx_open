/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_SETTING_H
#define NX_CLOUD_DB_SETTING_H

#include <list>

#include <QtCore/QSettings>

#include <utils/common/command_line_parser.h>
#include <utils/network/socket_common.h>

#include "db/types.h"


namespace nx {
namespace cdb {
namespace conf {

class Logging
{
public:
    QString logLevel;
    QString logDir;
};

/*!
    \note Values specified via command-line have priority over conf file (or win32 registry) values
*/
class Settings
{
public:
    Settings();

    bool showHelp() const;

    //!list of local endpoints to bind to. By default, 0.0.0.0:3346
    std::list<SocketAddress> endpointsToListen() const;
    QString dataDir() const;
    
    const Logging& logging() const;
    const db::ConnectionOptions& dbConnectionOptions() const;
    const QString& cloudBackendUrl() const;

    //!Loads settings from both command line and conf file (or win32 registry)
    void load( int argc, char **argv );
    //!Prints to std out
    void printCmdLineArgsHelp();

private:
    QnCommandLineParser m_commandLineParser;
    QSettings m_settings;
    bool m_showHelp;
    QString m_logLevel;

    Logging m_logging;
    db::ConnectionOptions m_dbConnectionOptions;
    QString m_cloudBackendUrl;

    void fillSupportedCmdParameters();
    void loadConfiguration();
};

}   //conf
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_SETTING_H
