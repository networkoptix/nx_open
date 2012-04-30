#include <qtsinglecoreapplication.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include "qtservice.h"
#include "proxy_listener.h"
#include "version.h"
#include "utils/common/util.h"
#include "utils/common/log.h"
#include "signal.h"

static const char SERVICE_NAME[] = "Network Optix Media Proxy";

class QnMain;
static QnMain* serviceMainInstance = 0;
void stopServer(int signal);


#ifdef Q_OS_WIN
#include <windows.h>
#include <stdio.h>
#include "utils/common/command_line_parser.h"
BOOL WINAPI stopServer_WIN(DWORD dwCtrlType)
{
    stopServer(dwCtrlType);
    return true;
}
#endif

static QtMsgHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const char *msg)
{
    if (defaultMsgHandler)
        defaultMsgHandler(type, msg);

    qnLogMsgHandler(type, msg);
}

QString getDataDirectory()
{
#ifdef Q_OS_LINUX
    return "/opt/networkoptix/mediaproxy/var";
#else
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
}

int getProxyPort()
{
#if defined(Q_OS_LINUX)
    QSettings qSettings("/opt/networkoptix/entcontroller/etc/entcontroller.conf", QSettings::IniFormat);
#else
    QSettings qSettings(QSettings::SystemScope, ORGANIZATION_NAME, "Enterprise Controller");
#endif
    return qSettings.value("proxy_port").toInt();
}

int serverMain(int argc, char *argv[])
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    signal(SIGINT, stopServer);
    signal(SIGABRT, stopServer);
    signal(SIGTERM, stopServer);

//    av_log_set_callback(decoderLogCallback);

    QCoreApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QCoreApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(qApp->argv()[0])).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));

    QString logFileName = dataLocation + QLatin1String("/log/log_file");

    if (!cl_log.create(logFileName, 1024*1024*10, 5, cl_logDEBUG1))
    {
        qApp->quit();

        return 0;
    }

    QnCommandLineParser commandLinePreParser;
    commandLinePreParser.addParameter(QnCommandLineParameter(QnCommandLineParameter::String, "--log-level", NULL, NULL));
    commandLinePreParser.parse(argc, argv);

    QnLog::initLog(commandLinePreParser.value("--log-level").toString());
    cl_log.log(APPLICATION_NAME, " started", cl_logALWAYS);
    cl_log.log("Software version: ", APPLICATION_VERSION, cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMsgHandler(myMsgHandler);

    // ------------------------------------------
    return 0;
}

class QnMain : public CLLongRunnable
{
public:
    QnMain(int argc, char* argv[])
        : m_argc(argc),
        m_argv(argv),
        m_proxyListener(0)
    {
        serviceMainInstance = this;
    }

    ~QnMain()
    {
        quit();
        stop();
        stopObjects();
    }

    void stopObjects()
    {
        if (m_proxyListener)
        {
            delete m_proxyListener;
            m_proxyListener = 0;
        }
    }

    void updatePort()
    {
        if (m_proxyListener) {
            int port = getProxyPort();
            m_proxyListener->updatePort(port ? port : DEFAUT_PROXY_PORT);
        }
    }

    void run()
    {
        int port = getProxyPort();
        m_proxyListener = new QnProxyListener(QHostAddress::Any, port ? port : DEFAUT_PROXY_PORT);
        m_proxyListener->start();

        exec();
    }

private:
    int m_argc;
    char** m_argv;

    QnProxyListener* m_proxyListener;
};

static const int COMMAND_UPDATE_CONFIG = 1001;

class QnMediaProxyService : public QtService<QtSingleCoreApplication>
{

public:
    QnMediaProxyService(int argc, char **argv)
        : QtService<QtSingleCoreApplication>(argc, argv, SERVICE_NAME),
        m_main(argc, argv)
    {
        setServiceDescription(SERVICE_NAME);
    }

protected:
    void start()
    {
        QtSingleCoreApplication *app = application();

        if (app->isRunning())
        {
            cl_log.log("Proxy server already started", cl_logERROR);
            qApp->quit();
            return;
        }

        serverMain(app->argc(), app->argv());
        m_main.start();
    }

    void stop()
    {
        stopServer(0);
    }

    virtual void processCommand( int code ) override
    {
        if (code == COMMAND_UPDATE_CONFIG)
        {
            m_main.updatePort();
        }
    }

private:
    QnMain m_main;
};

void stopServer(int)
{
    qApp->quit();
}

int main(int argc, char* argv[])
{
    QnMediaProxyService service(argc, argv);
    int result = service.exec();
    if (serviceMainInstance)
    {
        serviceMainInstance->stopObjects();
        serviceMainInstance = 0;
    }

    return result;
}
