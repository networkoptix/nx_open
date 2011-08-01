//#include <vld.h>
#include "eve_app.h"

#include "version.h"
#include "util.h"
#include "mainwnd.h"
#include "serial.h"
#include "settings.h"

#include "device/asynch_seacher.h"
#include "base/log.h"
#include "decoders/video/ipp_h264_decoder.h"
#include "device_plugins/arecontvision/devices/av_device_server.h"
#include "device_plugins/fake/devices/fake_device_server.h"

#include "ui/device_settings/dlg_factory.h"
#include "ui/device_settings/plugins/arecontvision/arecont_dlg.h"
#include "device/device_managmen/device_manager.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui/context_menu_helper.h"
#include "decoders/video/abstractdecoder.h"
#include "device_plugins/avigilon/devices/avigilon_device_server.h"
#include "device_plugins/android/devices/android_device_server.h"
#include "device_plugins/iqeye/devices/iqeye_device_server.h"
#include "device_plugins/desktop/device/desktop_device_server.h"

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

    cl_log.log("FFMPEG ", szMsg, cl_logERROR);
}

#ifndef UNICLIENT_TESTS

#include "ui/videoitem/timeslider.h"

int main(int argc, char *argv[])
{
//    av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(ORGANIZATION_NAME);
    QApplication::setApplicationName(APPLICATION_NAME);
    QApplication::setApplicationVersion(APPLICATION_VERSION);

    EveApplication application(argc, argv);

//    TimeSlider s;
//    s.setMaximumValue((qint64)411601284504748032LL);
//    s.show();

    QString argsMessage;
    for (int i = 1; i < argc; ++i)
    {
        argsMessage += fromNativePath(QString::fromLocal8Bit(argv[i]));
        if (i < argc-1)
            argsMessage += QChar('\0');
    }

    while (application.isRunning())
    {
        if (application.sendMessage(argsMessage))
            return 0;
    }

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(argv[0]).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + "/log");

    if (!cl_log.create(dataLocation + "/log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
    {
        application.quit();

        return 0;
    }

#ifdef _DEBUG
     cl_log.setLogLevel(cl_logDEBUG1);
    //cl_log.setLogLevel(cl_logWARNING);
#else
    cl_log.setLogLevel(cl_logWARNING);
#endif

    CL_LOG(cl_logALWAYS)
    {
        cl_log.log("\n\n========================================", cl_logALWAYS);
        cl_log.log(cl_logALWAYS, "Software version %s", APPLICATION_VERSION);
        cl_log.log(argv[0], cl_logALWAYS);
    }

    Settings& settings = Settings::instance();
    settings.load(getDataDirectory() + "/settings.xml");

    if (!settings.isAfterFirstRun() && !getMoviesDirectory().isEmpty())
        settings.addAuxMediaRoot(getMoviesDirectory());

    settings.save();

    cl_log.log("Using " + settings.mediaRoot() + " as media root directory", cl_logALWAYS);

    CLDevice::startCommandProc();

    CLDeviceManager::instance(); // to initialize net state;

    //===========================================================================
    //IPPH264Decoder::dll.init();

    CLVideoDecoderFactory::setCodecManufacture(CLVideoDecoderFactory::FFMPEG);

    //============================
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AVDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AVigilonDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AndroidDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&IQEyeDeviceServer::instance());
#ifdef Q_OS_WIN
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&DesktopDeviceServer::instance());
#endif // Q_OS_WIN

    CLDeviceSettingsDlgFactory::instance().registerDlgManufacture(&AreconVisionDlgManufacture::instance());
    //============================

    //=========================================================

    qApp->setStyleSheet("\
        QMenu {\
        background-color: black;\
        font-family: Bodoni MT;\
        font-size: 18px;\
        border: 1px solid rgb(110, 110, 110);\
        }\
        QMenu::item{\
        color: white;\
        padding-top: 4px;\
        padding-left: 5px;\
        padding-right: 15px;\
        padding-bottom: 4px;\
        }\
        QMenu::item:disabled{\
        color: rgb(110, 110, 110);\
        }\
        QMenu::item:selected {\
        background: rgb(40, 40, 40);\
        }\
        QMenu::separator {\
        height: 3px; \
        background: rgb(20, 20, 20);\
        }");

    /**/
    //=========================================================

    CLDeviceList recorders = CLDeviceManager::instance().getRecorderList();
    foreach(CLDevice* dev, recorders)
    {
        QString id = dev->getUniqueId();
        CLSceneLayoutManager::instance().addRecorderLayoutContent(id);
        dev->releaseRef();
    }

    //=========================================================

    initContextMenu();

    MainWnd mainWindow(argc, argv);
    mainWindow.show();

    QObject::connect(&application, SIGNAL(messageReceived(const QString&)),
        &mainWindow, SLOT(handleMessage(const QString&)));

    return application.exec();

    CLDevice::stopCommandProc();
}
#endif
