////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <iostream>

#include <QtSingleApplication>
#include <qtservice.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>

#include "launcher_fsm.h"
#include "version.h"

//!if defined, launcher compiles as daemon (linux) or win32 service
//#define I_AM_DAEMON

#ifdef _WIN32
#include <windows.h>

static BOOL WINAPI stopServer_WIN(DWORD /*dwCtrlType*/)
{
    QCoreApplication* app = QCoreApplication::instance();
    if( app )
        app->quit();
    return true;
}
#endif

QSettings qSettings;	//TODO/FIXME remove this shit. Have to add to build common as shared object, since it requires extern qSettibns to be defined somewhere...

static QString SERVICE_NAME = QString::fromLatin1("%1%2").arg(QLatin1String(QN_CUSTOMIZATION_NAME)).arg(QLatin1String("AppLauncher"));

#ifdef I_AM_DAEMON
class LauncherService
:
    public QtService<QtSingleApplication>
{
public:
    LauncherService( int argc, char **argv )
    : 
        QtService<QtSingleApplication>(argc, argv, SERVICE_NAME),
        m_argc( argc ),
        m_argv( argv )
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    virtual void start() override
    {
        QObject::connect( &m_fsm, SIGNAL(finished()), application(), SLOT(quit()) );
        QObject::connect( &m_fsm, SIGNAL(stopped()), application(), SLOT(quit()) );
        m_fsm.start();
    }

    virtual void stop() override
    {
        m_fsm.stop();
        application()->quit();
    }

private:
    LauncherFSM m_fsm;
    int m_argc;
    char** m_argv;
};
#endif


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

    cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);


#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif

#ifdef I_AM_DAEMON
    LauncherService service( argc, argv );
    return service.exec();
#else
    QtSingleApplication app( SERVICE_NAME, argc, argv );
    LauncherFSM fsm( quitMode );
    QObject::connect( &fsm, SIGNAL(finished()), &app, SLOT(quit()) );
    QObject::connect( &fsm, SIGNAL(stopped()), &app, SLOT(quit()) );
    fsm.start();
    return app.exec();
#endif
}
