////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <iostream>

#include <QtSingleApplication>
#include <QDir>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>

#include "applauncher_process.h"
#include "launcher_fsm.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>

static ApplauncherProcess* applauncherProcessInstance = 0;

static BOOL WINAPI stopServer_WIN(DWORD /*dwCtrlType*/)
{
    if( applauncherProcessInstance )
        applauncherProcessInstance->pleaseStop();
    return TRUE;
}
#endif

QSettings qSettings;	//TODO/FIXME remove this shit. Have to add to build common as shared object, since it requires extern qSettibns to be defined somewhere...

static QString SERVICE_NAME = QString::fromLatin1("%1%2").arg(QLatin1String(QN_CUSTOMIZATION_NAME)).arg(QLatin1String("AppLauncher"));

static void printHelp()
{
    std::cout<<
        "Arguments:\n"
        "  --log-level={log level}. Can be ERROR, WARN, INFO, DEBUG, DEBUG2. By default, WARN\n"
        "  --log-file=/path/to/log/file. If not specified, no log is written\n"
        "  quit. Tells another running launcher to terminate\n"
        "  --help. This help message\n"
        "\n";
}

int main( int argc, char* argv[] )
{
    QString logLevel = "WARN";
    QString logFilePath;
    bool quitMode = false;
    bool displayHelp = false;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter( &logLevel, "--log-level", NULL, QString() );
    commandLineParser.addParameter( &logFilePath, "--log-file", NULL, QString() );
    commandLineParser.addParameter( &quitMode, "quit", NULL, QString(), QVariant(true) );
    commandLineParser.addParameter( &displayHelp, "--help", NULL, QString(), QVariant(true) );
    commandLineParser.parse( argc, argv, stderr );

    if( displayHelp )
    {
        printHelp();
        return 0;
    }

    //initialize logging based on args
    if( !logFilePath.isEmpty() && !logLevel.isEmpty() )
    {
        cl_log.create( logFilePath, 1024*1024*10, 5, cl_logDEBUG1 );
        QnLog::initLog( logLevel );
    }

    NX_LOG( QN_APPLICATION_NAME" started", cl_logALWAYS );
    NX_LOG( "Software version: "QN_APPLICATION_VERSION, cl_logALWAYS );
    NX_LOG( "Software revision: "QN_APPLICATION_REVISION, cl_logALWAYS );

    QtSingleApplication app( SERVICE_NAME, argc, argv );
    QDir::setCurrent( QCoreApplication::applicationDirPath() );

    ApplauncherProcess applauncherProcess( quitMode );
    applauncherProcessInstance = &applauncherProcess;

#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif

    return applauncherProcess.run();
}
