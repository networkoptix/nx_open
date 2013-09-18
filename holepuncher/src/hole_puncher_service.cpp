/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "hole_puncher_service.h"

#include <iostream>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/network/aio/aioservice.h>

#include "version.h"


HolePuncherService::HolePuncherService( int argc, char **argv )
: 
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void HolePuncherService::pleaseStop()
{
    application()->quit();
}

int HolePuncherService::executeApplication()
{ 
    int result = application()->exec();
    deinitialize();
    return result;
}

void HolePuncherService::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }

    if( !initialize() )
        application->quit();
}

void HolePuncherService::stop()
{
    application()->quit();
}

bool HolePuncherService::initialize()
{
    //reading settings
#ifdef _WIN32
    m_settings.reset( new QSettings(QSettings::SystemScope, QN_ORGANIZATION_NAME, QN_APPLICATION_NAME) );
#else
    const QString configFileName = QString("/opt/%1/%2/etc/%2.conf").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    m_settings.reset( new QSettings(configFileName, QSettings::IniFormat) );
#endif

    const QString& dataLocation = getDataDirectory();

    //parsing command line arguments
    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), "ERROR");
    commandLineParser.parse(m_argc, m_argv, stderr);

    //logging
    if( logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = m_settings->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
        QDir().mkpath( logDir );
        const QString& logFileName = logDir + QLatin1String("/log_file");
        if( cl_log.create(logFileName, 1024*1024*10, 5, cl_logDEBUG1) )
            QnLog::initLog(logLevel);
        else
            std::wcerr<<L"Failed to create log file "<<logFileName.toStdWString()<<std::endl;
    }

    //TODO/IMPL starting aio


    //TODO/IMPL listening for incoming requests

    return true;
}

void HolePuncherService::deinitialize()
{
    //TODO/IMPL
}

QString HolePuncherService::getDataDirectory()
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
