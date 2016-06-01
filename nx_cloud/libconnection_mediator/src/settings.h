/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CONNECTION_MEDIATOR_SETTING_H
#define NX_CONNECTION_MEDIATOR_SETTING_H

#include <chrono>
#include <list>
#include <map>

#include <utils/common/command_line_parser.h>
#include <utils/common/settings.h>
#include <utils/db/types.h>
#include <utils/email/email.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/socket_common.h>


namespace nx {
namespace hpm {
namespace conf {

class General
{
public:
    QString systemUserToRunUnder;
    QString dataDir;
};

class Logging
{
public:
    QString logLevel;
    QString logDir;
};

class CloudDB
{
public:
    bool runWithCloud;
    QString endpoint;
    QString user;
    QString password;
    std::chrono::seconds updateInterval;

    CloudDB()
    :
        runWithCloud(true)
    {
    }
};

class Stun
{
public:
    std::list<SocketAddress> addrToListenList;
};

class Http
{
public:
    std::list<SocketAddress> addrToListenList;
};


/*!
    \note Values specified via command-line have priority over conf file (or win32 registry) values
*/
class Settings
{
public:
    Settings();

    bool showHelp() const;

    const General& general() const;
    const Logging& logging() const;
    const CloudDB& cloudDB() const;
    const Stun& stun() const;
    const Http& http() const;
    /** Properties for cloud connections */
    const api::ConnectionParameters& connectionParameters() const;

    //!Loads settings from both command line and conf file (or win32 registry)
    void load(int argc, char **argv);
    //!Prints to std out
    void printCmdLineArgsHelp();

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelp;

    General m_general;
    Logging m_logging;
    CloudDB m_cloudDB;
    Stun m_stun;
    Http m_http;
    api::ConnectionParameters m_connectionParameters;

    void fillSupportedCmdParameters();
    void loadConfiguration();
    void readEndpointList(
        const QString& str,
        std::list<SocketAddress>* const addrToListenList);
};

}   //conf
}   //hpm
}   //nx

#endif  //NX_CONNECTION_MEDIATOR_SETTING_H
