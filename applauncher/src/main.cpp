////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <QtSingleApplication>
#include <qtservice.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>

#include "launcher_fsm.h"
#include "version.h"

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
        QString logLevel;
        QString logFilePath;

        QnCommandLineParser commandLineParser;
        commandLineParser.addParameter( &logLevel, "--log-level", NULL, QString() );
        commandLineParser.addParameter( &logFilePath, "--log-file", NULL, QString() );
        commandLineParser.parse( m_argc, m_argv, stderr );

        //initialize logging based on args
        if( !logFilePath.isEmpty() && !logLevel.isEmpty() )
        {
            cl_log.create( logFilePath, 1024*1024*10, 5, cl_logDEBUG1 );
            QnLog::initLog( logLevel );
        }

        cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
        cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
        cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);

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


int main( int argc, char* argv[] )
{
    LauncherService service( argc, argv );
#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    return service.exec();
}
