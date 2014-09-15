/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "hole_puncher_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/network/aio/aioservice.h>
#include <utils/common/systemerror.h>

#include "version.h"


HolePuncherProcess::HolePuncherProcess( int argc, char **argv )
: 
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void HolePuncherProcess::pleaseStop()
{
    application()->quit();
}

int HolePuncherProcess::executeApplication()
{
    if( !initialize() )
        return 1;

    const int result = application()->exec();

    deinitialize();
    return result;
}

void HolePuncherProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }
}

void HolePuncherProcess::stop()
{
    application()->quit();
}

static int printHelp()
{
    //TODO/IMPL

    return 0;
}

bool HolePuncherProcess::initialize()
{
    static const QLatin1String DEFAULT_LOG_LEVEL( "ERROR" );
    static const QLatin1String DEFAULT_ADDRESS_TO_LISTEN( ":3345" );


    //reading settings
#ifdef _WIN32
    m_settings.reset( new QSettings(QSettings::SystemScope, QnVersion::organizationName(), QN_APPLICATION_NAME) );
#else
    const QString configFileName = QString("/opt/%1/%2/etc/%2.conf").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    m_settings.reset( new QSettings(configFileName, QSettings::IniFormat) );
#endif

    const QString& dataLocation = getDataDirectory();

    //parsing command line arguments
    bool showHelp = false;
    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), false);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), DEFAULT_LOG_LEVEL);
    commandLineParser.parse(m_argc, m_argv, stderr);

    if( showHelp )
        return printHelp();

    //logging
    if( logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = m_settings->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
        QDir().mkpath( logDir );
        const QString& logFileName = logDir + QLatin1String("/log_file");
        if( cl_log.create( logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1 ) )
            QnLog::initLog( logLevel );
        else
            std::wcerr<<L"Failed to create log file "<<logFileName.toStdWString()<<std::endl;
        NX_LOG( lit( "=================================================================================" ), cl_logALWAYS );
        NX_LOG( lit( "%1 started" ).arg( QN_APPLICATION_NAME ), cl_logALWAYS );
        NX_LOG( lit( "Software version: %1" ).arg( QN_APPLICATION_VERSION ), cl_logALWAYS );
        NX_LOG( lit( "Software revision: %1" ).arg( QN_APPLICATION_REVISION ), cl_logALWAYS );
    }

    const QStringList& addrToListenStrList = m_settings->value("addressToListen", DEFAULT_ADDRESS_TO_LISTEN).toString().split(',');
    std::list<SocketAddress> addrToListenList;
    std::transform(
        addrToListenStrList.begin(),
        addrToListenStrList.end(),
        std::back_inserter(addrToListenList),
        [] (const QString& str) { return SocketAddress(str); } );
    if( addrToListenList.empty() )
    {
        NX_LOG( "No address to listen", cl_logALWAYS );
        return false;
    }

    //m_multiAddressStunServer.reset( new MultiAddressServer<StunStreamSocketServer>(std::move(addrToListenList)) );
    m_multiAddressHttpServer.reset( new MultiAddressServer<nx_http::HttpStreamSocketServer>(std::move(addrToListenList)) );

    //if( !m_multiAddressStunServer->bind() )
    //    return false;
    if( !m_multiAddressHttpServer->bind() )
        return false;

    //TODO: #ak process privilege reduction should be made here

    //if( !m_multiAddressStunServer->listen() )
    //    return false;
    if( !m_multiAddressHttpServer->listen() )
        return false;

    return true;
}

void HolePuncherProcess::deinitialize()
{
    //stopping accepting incoming connections
    m_multiAddressStunServer.reset();
    m_multiAddressHttpServer.reset();
}

QString HolePuncherProcess::getDataDirectory()
{
    const QString& dataDirFromSettings = m_settings->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    QString varDirName = qSettings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}
