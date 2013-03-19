////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <QtSingleApplication>
#include <qtservice.h>

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
        QtService<QtSingleApplication>(argc, argv, SERVICE_NAME)
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    //virtual int executeApplication() override
    //{ 
    //    return application()->exec();
    //}

    virtual void start() override
    {
        cl_log.create( "C:/launcher.log", 1024*1024*10, 5, cl_logDEBUG1 );

        QnLog::initLog("DEBUG2");
        cl_log.log(QN_APPLICATION_NAME, " started", cl_logALWAYS);
        cl_log.log("Software version: ", QN_APPLICATION_VERSION, cl_logALWAYS);
        cl_log.log("Software revision: ", QN_APPLICATION_REVISION, cl_logALWAYS);

        QObject::connect( &m_fsm, SIGNAL(finished()), application(), SLOT(quit()) );
        QObject::connect( &m_fsm, SIGNAL(stopped()), application(), SLOT(quit()) );
        m_fsm.start();

        int ms = 20;
#if defined(Q_OS_WIN)
        ::Sleep(DWORD(ms));
#else
        struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
        nanosleep(&ts, NULL);
#endif
    }

    virtual void stop() override
    {
        m_fsm.stop();
        application()->quit();
#if 0
        m_main.exit();
        m_main.wait();
        stopServer(0);
#endif
    }

private:
    LauncherFSM m_fsm;
};


int main( int argc, char* argv[] )
{
    LauncherService service( argc, argv );
#ifdef _WIN32
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    return service.exec();
}
