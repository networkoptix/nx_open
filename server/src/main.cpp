#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include "version.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "core/resourcemanagment/asynch_seacher.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/sleep.h"
#include "rtsp/rtsp_listener.h"
#include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
#include "recorder/recording_manager.h"
#include "recorder/storage_manager.h"
#include "api/AppServerConnection.h"
#include "appserver/processor.h"
#include "recording/file_deletor.h"
#include "rest/server/rest_server.h"
#include "rest/handlers/recorded_chunks.h"
#include "core/resource/video_server.h"
#include <signal.h>
#include <xercesc/util/PlatformUtils.hpp>
#include "core/misc/scheduleTask.h"
#include "qtservice.h"
#include "eventmanager.h"

#include <fstream>
#include "plugins/resources/axis/axis_resource_searcher.h"
#include "plugins/resources/d-link/dlink_resource_searcher.h"

//#include "device_plugins/arecontvision/devices/av_device_server.h"

//#define TEST_RTSP_SERVER

static const int DEFAUT_RTSP_PORT = 50000;

QMutex global_ffmpeg_mutex;

void decoderLogCallback(void* /*pParam*/, int i, const char* szFmt, va_list args)
{
    //USES_CONVERSION;

    //Ignore debug and info (i == 2 || i == 1) messages
    if(AV_LOG_ERROR != i)
    {
        //return;
    }

    // AVCodecContext* pCtxt = (AVCodecContext*)pParam;

    char szMsg[1024];
    vsprintf(szMsg, szFmt, args);
    //if(szMsg[strlen(szMsg)] == '\n')
    {
        szMsg[strlen(szMsg)-1] = 0;
    }

    cl_log.log(QLatin1String("FFMPEG "), QString::fromLocal8Bit(szMsg), cl_logERROR);
}

QHostAddress resolveHost(const QString& hostString)
{
    QHostAddress host(hostString);
    if (host.toIPv4Address() != 0)
        return host;

    QHostInfo info = QHostInfo::fromName(hostString);

    // Can't resolve
    if (info.error() != QHostInfo::NoError)
    {
        cl_log.log("Couldn't resolve host ", hostString, cl_logERROR);
        return QHostAddress();
    }

    // Initialize to zero
    host = QHostAddress();
    foreach (const QHostAddress &address, info.addresses())
    {
        if (address.toIPv4Address() != 0)
        {
            host = address;
            break;
        }
    }

    if (host.toIPv4Address() == 0)
        cl_log.log("No ipv4 address associated with host ", hostString, cl_logERROR);

    return host;
}


QString defaultLocalAddress(const QHostAddress& target)
{
    {
        QUdpSocket socket;
        socket.connectToHost(target, 53);

        if (socket.localAddress() != QHostAddress::LocalHost)
            return socket.localAddress().toString();
    }

    {
        // try select default interface

        QUdpSocket socket;
        socket.connectToHost("8.8.8.8", 53);

        return socket.localAddress().toString();
    }
}

QString localMac(const QString& myAddress)
{
    foreach (const QNetworkInterface &interface, QNetworkInterface::allInterfaces())
    {
        if (interface.allAddresses().contains(QHostAddress(myAddress)))
            return interface.hardwareAddress();
    }

    return "";
}

void ffmpegInit()
{
    avcodec_init();
    av_register_all();

    extern URLProtocol ufile_protocol;
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));
}

QString serverGuid()
{
    QSettings settings(QSettings::SystemScope, ORGANIZATION_NAME, APPLICATION_NAME);
    QString guid = settings.value("serverGuid").toString();

    if (guid.isEmpty())
    {
        if (!settings.isWritable())
        {
            return guid;
        }

        guid = QUuid::createUuid().toString();
        settings.setValue("serverGuid", guid);
    }

    return guid;
}

QnVideoServerPtr registerServer(QnAppServerConnectionPtr appServerConnection, const QString& myAddress)
{
    QSettings settings(QSettings::SystemScope, ORGANIZATION_NAME, APPLICATION_NAME);

    QnVideoServer server;

    // If there is already stored server with this guid, other parameters will be ignored
    server.setGuid(serverGuid());

    server.setName(QString("Server ") + myAddress);
    server.setUrl(QString("rtsp://") + myAddress + QString(':') + settings.value("rtspPort", DEFAUT_RTSP_PORT).toString());
    server.setApiUrl(QString("http://") + myAddress + QString(':') + settings.value("apiPort", DEFAULT_REST_PORT).toString());

    QnVideoServerList servers;
    QByteArray errorString;
    if (appServerConnection->registerServer(server, servers, errorString) != 0)
    {
        qDebug() << "registerServer(): Call to registerServer failed. Reason: " << errorString;

        return QnVideoServerPtr();
    }

    return servers.at(0);
}

void stopServer(int signal)
{
    QnResource::stopCommandProc();
    QnResourceDiscoveryManager::instance().stop();
    QnRecordingManager::instance()->stop();
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <stdio.h>
BOOL WINAPI stopServer_WIN(DWORD dwCtrlType)
{
    stopServer(dwCtrlType);
    return true;
}
#endif

int serverMain(int argc, char *argv[])
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    xercesc::XMLPlatformUtils::Initialize();

#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif
    signal(SIGINT, stopServer);
    signal(SIGABRT, stopServer);
    signal(SIGTERM, stopServer);

    Q_INIT_RESOURCE(api);

//    av_log_set_callback(decoderLogCallback);

    QCoreApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QCoreApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(qApp->argv()[0])).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));

    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
    {
        qApp->quit();

        return 0;
    }

#ifdef _DEBUG
     //cl_log.setLogLevel(cl_logDEBUG1);
    cl_log.setLogLevel(cl_logDEBUG1);
#else
    cl_log.setLogLevel(cl_logWARNING);
#endif

    CL_LOG(cl_logALWAYS)
    {
        cl_log.log(QLatin1String("\n\n========================================"), cl_logALWAYS);
        cl_log.log(cl_logALWAYS, "Software version %s", APPLICATION_VERSION);
        cl_log.log(QFile::decodeName(qApp->argv()[0]), cl_logALWAYS);
    }

    qInstallMsgHandler(qDebugCLLogHandler);

    QnResource::startCommandProc();

    QnResourcePool::instance(); // to initialize net state;
    ffmpegInit();


    // ------------------------------------------
#ifdef TEST_RTSP_SERVER
    addTestData();
#endif

    QDir stateDirectory;
    stateDirectory.mkpath(dataLocation + QLatin1String("/state"));
    qnFileDeletor->init(dataLocation + QLatin1String("/state")); // constructor got root folder for temp files

    return 0;
}

void initAppServerConnection(const QSettings &settings)
{
    QUrl appServerUrl;

    // ### remove
    appServerUrl.setScheme(QLatin1String("http"));
    appServerUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerUrl.setUserName(settings.value("appserverLogin", QLatin1String("appserver")).toString());
    appServerUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());

    QnAppServerConnectionFactory::setDefaultUrl(appServerUrl);
    QnAppServerConnectionFactory::setDefaultFactory(&QnResourceDiscoveryManager::instance());
}

void initAppServerEventConnection(const QSettings &settings)
{
    QUrl appServerEventsUrl;

    // ### remove
    appServerEventsUrl.setScheme(QLatin1String("http"));
    appServerEventsUrl.setHost(settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
    appServerEventsUrl.setPort(settings.value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
    appServerEventsUrl.setUserName(settings.value("appserverLogin", QLatin1String("appserver")).toString());
    appServerEventsUrl.setPassword(settings.value("appserverPassword", QLatin1String("123")).toString());
    appServerEventsUrl.setPath("/events");

    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    QnEventManager* eventManager = QnEventManager::instance();
    eventManager->init(appServerEventsUrl, EVENT_RECONNECT_TIMEOUT);
}

class QnMain : public QThread
{
public:
    QnMain(int argc, char* argv[])
        : m_argc(argc),
        m_argv(argv),
        m_processor(0),
        m_rtspListener(0),
        m_restServer(0)
    {
    }

    ~QnMain()
    {
        if (m_restServer)
            delete m_restServer;

        if (m_rtspListener)
            delete m_rtspListener;

        if (m_processor)
            delete m_processor;
    }

    void run()
    {
        // Use system scope
        QSettings settings(QSettings::SystemScope, ORGANIZATION_NAME, APPLICATION_NAME);

        initAppServerConnection(settings);
        initAppServerEventConnection(settings);
        QnEventManager* eventManager = QnEventManager::instance();

        QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();

        initResourceTypes(appServerConnection);

        QString appserverHostString = settings.value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString();
        
        QHostAddress appserverHost;
        do
        {
            appserverHost = resolveHost(appserverHostString);
        } while (appserverHost.toIPv4Address() == 0);
        
        QnVideoServerPtr videoServer;

        while (videoServer.isNull())
        {
            videoServer = registerServer(appServerConnection, defaultLocalAddress(appserverHost));
            if (videoServer.isNull())
                QnSleep::msleep(1000);
        }

        eventManager->run();


        m_processor = new QnAppserverResourceProcessor(videoServer->getId());

        QUrl rtspUrl(videoServer->getUrl());
        QUrl apiUrl(videoServer->getApiUrl());

        m_rtspListener = new QnRtspListener(QHostAddress::Any, rtspUrl.port());
        m_rtspListener->start();

        m_restServer = new QnRestServer(QHostAddress::Any, apiUrl.port());
        m_restServer->registerHandler("api/RecordedTimePeriods", new QnRecordedChunkListHandler());
        m_restServer->registerHandler("xsd/*", new QnXsdHelperHandler());

        // Get storages sample code.
        QnResourceList storages;
        QByteArray errorString;
        while (appServerConnection->getStorages(storages, errorString) != 0)
        {
            qDebug() << "QnMain::run(): Can't get storages. Reason: " << errorString;
            QnSleep::msleep(1000);
        }

        bool storageAdded = false;
        foreach (QnResourcePtr resource, storages)
        {
            QnStoragePtr storage = resource.dynamicCast<QnStorage>();
            if (storage->getParentId() == videoServer->getId())
            {
                storageAdded = true;
                qnResPool->addResource(storage);
                qnStorageMan->addStorage(storage);
            }
        }

        if (!storageAdded)
        {
            cl_log.log("Creating new storage", cl_logINFO);

            QnStoragePtr storage(new QnStorage());
            storage->setParentId(videoServer->getId());
            storage->setName("Initial");
            storage->setUrl(settings.value("mediaDir", "c:/records").toString().replace("\\", "/"));
            storage->setSpaceLimit(5ll * 1024 * 1024 * 1024);

            QByteArray errorString;
            if (appServerConnection->addStorage(*storage, errorString))
                qDebug() << "Couldn't add storage: " << errorString;

            qnResPool->addResource(storage);
            qnStorageMan->addStorage(storage);
        }
        qnStorageMan->loadFullFileCatalog();

        QnRecordingManager::instance()->start();
        // ------------------------------------------


        //===========================================================================
        //IPPH264Decoder::dll.init();

        //============================
        QnResourceDiscoveryManager::instance().setResourceProcessor(m_processor);
        QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlArecontResourceSearcher::instance());
        QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlAxisResourceSearcher::instance());
        QnResourceDiscoveryManager::instance().addDeviceServer(&QnPlDlinkResourceSearcher::instance());


        //CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
        //CLDeviceSearcher::instance()->addDeviceServer(&IQEyeDeviceServer::instance());

        QnScheduleTaskList scheduleTasks;

        errorString.clear();
        QnSecurityCamResourceList cameras;
        while (appServerConnection->getCameras(cameras, videoServer->getId(), errorString) != 0)
        {
            qDebug() << "QnMain::run(): Can't get cameras. Reason: " << errorString;
            QnSleep::msleep(1000);
        }

        foreach(QnSecurityCamResourcePtr camera, cameras)
        {
            qDebug() << "Connecting resource: " << camera->getName();
            QObject::connect(camera.data(), SIGNAL(onStatusChanged(QnResource::Status, QnResource::Status)), m_processor, SLOT(onResourceStatusChanged(QnResource::Status, QnResource::Status)));

            qnResPool->addResource(camera);

            QnRecordingManager::instance()->updateSchedule(camera);
        }

        foreach (QnScheduleTask scheduleTask, scheduleTasks)
        {
            QString str;
            QTextStream stream(&str);

            stream << "ScheduleTask " << scheduleTask.m_id.toString() <<
                scheduleTask.m_afterThreshold <<
                scheduleTask.m_beforeThreshold <<
                scheduleTask.m_dayOfWeek <<
                scheduleTask.m_doRecordAudio <<
                scheduleTask.m_startTime <<
                scheduleTask.m_endTime <<
                scheduleTask.m_recordType <<
                scheduleTask.m_sourceId.toString();
            cl_log.log(str, cl_logALWAYS);
        }

        QnResourceDiscoveryManager::instance().start();

        exec();
    }

private:
    int m_argc;
    char** m_argv;

    QnAppserverResourceProcessor* m_processor;
    QnRtspListener* m_rtspListener;
    QnRestServer* m_restServer;
};

class QnVideoService : public QtService<QCoreApplication>
{
public:
    QnVideoService(int argc, char **argv)
        : QtService<QCoreApplication>(argc, argv, "Network Optix VMS Media Server"),
        m_main(argc, argv)
    {
        setServiceDescription("Network Optix VMS Media Server.");
    }

protected:
    void start()
    {
        QCoreApplication *app = application();
        QString guid = serverGuid();

        if (guid.isEmpty())
        {
            cl_log.log("Can't save guid. Run once as administrator.", cl_logERROR);
            qApp->quit();
            return;
        }

        serverMain(app->argc(), app->argv());
        m_main.start();
    }

    void stop()
    {
        stopServer(0);
        xercesc::XMLPlatformUtils::Terminate ();
    }

private:
    QnMain m_main;
};

int main(int argc, char* argv[])
{
    QnVideoService service(argc, argv);
    return service.exec();
}
