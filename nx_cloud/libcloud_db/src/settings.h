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


namespace cdb
{

class Settings
{
public:
    Settings();

    bool showHelp() const;

    QString logLevel() const;
    //!list of local endpoints to bind to. By default, 0.0.0.0:3346
    std::list<SocketAddress> endpointsToListen() const;
    QString dataDir() const;
    QString logDir() const;

    void parseArgs( int argc, char **argv );
    //!Prints to std out
    void printCmdLineArgsHelp();

private:
    QnCommandLineParser m_commandLineParser;
    QSettings m_settings;
    bool m_showHelp;
    QString m_logLevel;

    void fillSupportedCmdParameters();
};

}   //cdb

#endif  //NX_CLOUD_DB_SETTING_H
