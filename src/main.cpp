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

    cl_log.log(QLatin1String("FFMPEG "), QString::fromLocal8Bit(szMsg), cl_logERROR);
}

#ifndef UNICLIENT_TESTS

#include "ui/videoitem/timeslider.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

//    av_log_set_callback(decoderLogCallback);

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    EveApplication application(argc, argv);

    QString argsMessage;
    for (int i = 1; i < argc; ++i)
    {
        argsMessage += fromNativePath(QString::fromLocal8Bit(argv[i]));
        if (i < argc-1)
            argsMessage += QLatin1Char('\0'); // ### QString doesn't support \0 in the string
    }

    while (application.isRunning())
    {
        if (application.sendMessage(argsMessage))
            return 0;
    }

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));

    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
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
        cl_log.log(QLatin1String("\n\n========================================"), cl_logALWAYS);
        cl_log.log(cl_logALWAYS, "Software version %s", APPLICATION_VERSION);
        cl_log.log(QFile::decodeName(argv[0]), cl_logALWAYS);
    }

    Settings& settings = Settings::instance();
    settings.load(getDataDirectory() + QLatin1String("/settings.xml"));

    if (!settings.isAfterFirstRun() && !getMoviesDirectory().isEmpty())
        settings.addAuxMediaRoot(getMoviesDirectory());

    settings.save();

    cl_log.log(QLatin1String("Using ") + settings.mediaRoot() + QLatin1String(" as media root directory"), cl_logALWAYS);

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

    CLDeviceSettingsDlgFactory::registerDlgManufacture(&AreconVisionDlgManufacture::instance());
    //============================

    qApp->setQuitOnLastWindowClosed(true);

    //=========================================================

#ifndef Q_OS_DARWIN
    qApp->setStyle(QLatin1String("Bespin"));
#endif

    qApp->setStyleSheet(QLatin1String(
            "QMenu {\n"
            "font-family: Bodoni MT;\n"
            "font-size: 18px;\n"
            "}"));

    /*qApp->setStyleSheet(QLatin1String(
        "QMenu {\n"
        "background-color: black;\n"
        "font-family: Bodoni MT;\n"
        "font-size: 18px;\n"
        "border: 1px solid rgb(110, 110, 110);\n"
        "}\n"
        "QMenu::item{\n"
        "color: white;\n"
        "padding-top: 4px;\n"
        "padding-left: 5px;\n"
        "padding-right: 15px;\n"
        "padding-bottom: 4px;\n"
        "}\n"
        "QMenu::item:disabled{\n"
        "color: rgb(110, 110, 110);\n"
        "}\n"
        "QMenu::item:selected {\n"
        "background: rgb(40, 40, 40);\n"
        "}\n"
        "QMenu::separator {\n"
        "height: 3px;\n"
        "background: rgb(20, 20, 20);\n"
        "}"));*/

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

    int result = application.exec();

    CLDevice::stopCommandProc();

    return result;
}
#endif
