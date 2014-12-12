#include "AxHDWitness.h"

#include "client/client_message_processor.h"
#include "api/session_manager.h"
#include "core/resource_management/resource_discovery_manager.h"

#include "common/common_module.h"

#include "ui/workbench/workbench_navigator.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_layout.h"

#include <QAxAggregated>
#include <QAxFactory>
#include <objsafe.h>

#include "core/resource_management/resource_pool.h"
#include "core/resource/layout_resource.h"
#include "core/resource/media_resource.h"
#include "core/resource/network_resource.h"

#include <fstream>

#include "version.h"
#include "ui/widgets/main_window.h"
#include "client/client_settings.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtWidgets/QDesktopWidget>

#include "decoders/video/ipp_h264_decoder.h"

#include <utils/common/command_line_parser.h>
#include <utils/common/app_info.h>
// ax23 #include "ui/device_settings/dlg_factory.h"
#include "ui/actions/action_manager.h"
#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "decoders/video/abstractdecoder.h"
// ax23 #include "device_plugins/desktop/device/desktop_device_server.h"
#include "libavformat/avio.h"
#include "utils/common/util.h"
// ax23 #include "plugins/resources/archive/avi_files/avi_resource.h"
// ax23 #include "core/resourcemanagment/resource_pool.h"
// ax23 #include "utils/client_util.h"
// ax23 #include "plugins/resources/arecontvision/resource/av_resource_searcher.h"
// ax23 #include "api/app_server_connection.h"
// ax23 #include "device_plugins/server_camera/appserver.h"
// ax23 #include "utils/util.h"

// ax23 #include "core/resource/video_server.h"
#include "core/resource/storage_resource.h"

// ax23 #include "plugins/resources/axis/axis_resource_searcher.h"
#include "core/resource/resource_directory_browser.h"

#include "tests/auto_tester.h"
// ax23 #include "plugins/resources/d-link/dlink_resource_searcher.h"
// ax23 #include "plugins/resources/droid/droid_resource_searcher.h"
// ax23 #include "ui/actions/action_manager.h"
// ax23 #include "ui/tooltips/tool_tip.h"
// ax23 #include "plugins/resources/iqinvision/iqinvision_resource_searcher.h"
// ax23 #include "plugins/resources/droid_ipwebcam/ipwebcam_droid_resource_searcher.h"
// ax23 #include "plugins/resources/isd/isd_resource_searcher.h"
// ax23 #include "plugins/resources/onvif/onvif_resource_searcher_wsdd.h"
#include "utils/network/socket.h"
#include <openssl/evp.h>

#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "core/resource/camera_history.h"
//ax23 #include "clientmain.h"
//ax23 #include "utils/settings.h"
#include <utils/common/log.h>

#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench_display.h"

#include <QtCore/private/qthread_p.h>

#include <QXmlStreamWriter>

static QtMessageHandler defaultMsgHandler = 0;

static int lockmgr(void **mtx, enum AVLockOp op)
{
    QMutex** qMutex = (QMutex**) mtx;
    switch(op) {
        case AV_LOCK_CREATE:
            *qMutex = new QMutex();
            return 0;
        case AV_LOCK_OBTAIN:
            (*qMutex)->lock();
            return 0;
        case AV_LOCK_RELEASE:
            (*qMutex)->unlock();
            return 0;
        case AV_LOCK_DESTROY:
            delete *qMutex;
            return 0;
    }
    return 1;
}

void ffmpegInit()
{
    //avcodec_init();
    av_register_all();

    if(av_lockmgr_register(lockmgr) != 0)
    {
        qCritical() << "Failed to register ffmpeg lock manager";
    }

    // client uses ordinary QT file to access file system, server uses buffering access implemented inside QnFileStorageResource
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    // QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);
    //QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("memory"), QnLayoutFileStorageResource::instance);

    /*
    extern URLProtocol ufile_protocol;
    av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));

    extern URLProtocol qtufile_protocol;
    av_register_protocol2(&qtufile_protocol, sizeof(qtufile_protocol));
    */
}

static void myMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (defaultMsgHandler)
        defaultMsgHandler(type, ctx, msg);

    qnLogMsgHandler(type, ctx, msg);
}

extern HHOOK qax_hhook;

extern LRESULT QT_WIN_CALLBACK axs_FilterProc(int nCode, WPARAM wParam, LPARAM lParam);

LRESULT QT_WIN_CALLBACK qn_FilterProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    QThread *thread = QThread::currentThread();
    if(thread == NULL || QThreadData::get2(thread)->loopLevel == 0) { 
        return axs_FilterProc(nCode, wParam, lParam);
    } else {
        return CallNextHookEx(qax_hhook, nCode, wParam, lParam);
    }
}

AxHDWitness::AxHDWitness(QWidget* parent, const char* name)
    : m_mainWindow(0), m_isInitialized(false)
{
	Q_UNUSED(parent);
	Q_UNUSED(name);

    initialize();
}

AxHDWitness::~AxHDWitness()
{
    finalize();
}


bool AxHDWitness::event(QEvent *event) {
    switch(event->type()) {
    case QEvent::Show: {
        if(!m_mainWindow) {
            createMainWindow();

            QVBoxLayout *mainLayout = new QVBoxLayout;
            mainLayout->addWidget(m_mainWindow);
            setLayout(mainLayout);

            QMetaObject::invokeMethod(m_mainWindow, "showFullScreen", Qt::QueuedConnection);
        }
        break;
    }
    default:
        break;
    }

    bool result = QWidget::event(event);

    switch(event->type()) {
    case QEvent::WindowBlocked: {
        HWND hwnd = GetAncestor((HWND)this->winId(), GA_ROOT);
        EnableWindow(hwnd, FALSE);
        break;
    }
    case QEvent::WindowUnblocked: {
        HWND hwnd = GetAncestor((HWND)this->winId(), GA_ROOT);
        EnableWindow(hwnd, TRUE);
        break;
    }
    default:
        break;
    }

    return result;
}

void AxHDWitness::initialize()
{
    if (!m_isInitialized) {
        doInitialize();

        if(UnhookWindowsHookEx(qax_hhook))
            qax_hhook = SetWindowsHookEx(WH_GETMESSAGE, qn_FilterProc, 0, GetCurrentThreadId());

        m_isInitialized = true;
    }
}

void AxHDWitness::finalize()
{
    if (m_isInitialized) {
        doFinalize();
        m_isInitialized = false;
    }
}

void AxHDWitness::play()
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->setPlaying(true);
}

void AxHDWitness::setSpeed(double speed)
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->setSpeed(speed);
}

qint64 AxHDWitness::currentTime() 
{
    if(!m_mainWindow)
        return 0;

    return m_context->navigator()->position();
}

void AxHDWitness::setCurrentTime(qint64 time)
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->setPosition(time / 10000); // CY scaling...
}

void AxHDWitness::jumpToLive() 
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->setLive(true);
}

void AxHDWitness::pause()
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->setPlaying(false);
}

void AxHDWitness::nextFrame()
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->stepForward();
}

void AxHDWitness::prevFrame()
{
    if(!m_mainWindow)
        return;

    m_context->navigator()->stepBackward();
}

void AxHDWitness::clear()
{
    if(!m_mainWindow)
        return;

    m_context->workbench()->currentLayout()->clear();
}

void AxHDWitness::addToCurrentLayout(const QString &uniqueId) {
    QnResourcePtr resource = m_context->resourcePool()->getResourceByUniqId(uniqueId);
    if(!resource)
        return;

    m_context->menu()->trigger(Qn::DropResourcesAction, resource);
}

void AxHDWitness::removeFromCurrentLayout(const QString &uniqueId) {
    //QSet<QnWorkbenchItem *> items = m_context->workbench()->currentLayout()->items(uniqueId);
	auto layout = m_context->workbench()->currentLayout()->resource();
	if (layout)
		layout->removeItem(layout->getItem(uniqueId));
    //qDeleteAll(items);
}

QString AxHDWitness::resourceListXml() {
    QString result;
    
    QXmlStreamWriter writer(&result);
    writer.writeStartDocument();
    writer.writeStartElement(QLatin1String("resources"));

    foreach(const QnResourcePtr &resource, m_context->resourcePool()->getResources()) {
        QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
        if(!mediaResource)
            continue;

        writer.writeStartElement(QLatin1String("resource"));
        writer.writeAttribute(QLatin1String("uniqueId"), resource->getUniqueId());
        writer.writeAttribute(QLatin1String("isCamera"), QString::number((resource->flags() & Qn::live_cam) == Qn::live_cam));
        writer.writeAttribute(QLatin1String("name"), resource->getName());
        if(QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>())
            writer.writeAttribute(QLatin1String("ipAddress"), networkResource->getHostAddress());
        writer.writeEndElement();
    }

    writer.writeEndElement();

    return result;
}

void AxHDWitness::reconnect(const QString &url) {
    QnConnectionData data;
    data.url = url.isEmpty() ? QLatin1String("https://localhost:31338") : url;

    qnSettings->setLastUsedConnection(data);

    m_context->menu()->trigger(Qn::ReconnectAction);
}

void AxHDWitness::maximizeItem(const QString &uniqueId) {
    QSet<QnWorkbenchItem *> items = m_context->workbench()->currentLayout()->items(uniqueId);
    if(items.isEmpty())
        return;

    m_context->menu()->trigger(Qn::MaximizeItemAction, m_context->display()->widget(*items.begin()));
}

void AxHDWitness::unmaximizeItem(const QString &uniqueId) {
    QSet<QnWorkbenchItem *> items = m_context->workbench()->currentLayout()->items(uniqueId);
    if(items.isEmpty())
        return;

    m_context->menu()->trigger(Qn::UnmaximizeItemAction, m_context->display()->widget(*items.begin()));
}

void AxHDWitness::slidePanelsOut() {
    m_context->menu()->trigger(Qn::MaximizeAction);
}

bool AxHDWitness::doInitialize()
{
	// DebugBreak();

	AllowSetForegroundWindow(ASFW_ANY);

	int argc = 0;

    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths( pluginDirs );

	m_clientModule.reset(new QnClientModule(argc, NULL));
//x	QnResourcePool::initStaticInstance( new QnResourcePool() );

    /* Create application instance. */
    QApplication* application = qApp;

//x	QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin ? lit(":/skin_light") : lit(":/skin_dark");

//x    skin.reset(new QnSkin(QStringList() << lit(":/skin") << customizationPath));

//x    QnCustomization customization;
    //xcustomization.add(QnCustomization(skin->path("customization_common.json")));
    //xcustomization.add(QnCustomization(skin->path("customization_base.json")));
    //xcustomization.add(QnCustomization(skin->path("customization_child.json")));

    //xcustomizer.reset(new QnCustomizer(customization));
    //xcustomizer->customize(qnGlobals);

	
    application->setQuitOnLastWindowClosed(true);
//x    application->setWindowIcon(qnSkin->icon("logo_tray.png"));

    /* Initialize connections. */
    // initAppServerConnection();
    //xqnSettings->save();
//x    cl_log.log(QLatin1String("Using ") + qnSettings->mediaFolder() + QLatin1String(" as media root directory"), cl_logALWAYS);


    /* Initialize application instance. */
    application->setStartDragDistance(20);
    

//ax23    QnToolTip::instance();

#if 0 // AXFIXME
    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());
#endif

    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (!QDir().mkpath(dataLocation + QLatin1String("/log")))
        return false;
    
    QFileInfo fi(QAxFactory::serverFilePath());
    QString base = fi.baseName();

    if (!cl_log.create(dataLocation + QLatin1String("/log/ax_log_file") + base, 1024*1024*10, 5, cl_logDEBUG1))
        return false;

    QnLog::initLog(QString());
    cl_log.log(lit(QN_APPLICATION_NAME), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QnAppInfo::applicationVersion(), cl_logALWAYS);
#if 0 // AXFIXME
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);
#endif

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

	QnSessionManager::instance()->start();
//x	ffmpegInit();

//x    QnResourcePool::instance(); // to initialize net state;

//x    OpenSSL_add_all_digests(); // open SSL init

    //===========================================================================

    CLVideoDecoderFactory::setCodecManufacture(CLVideoDecoderFactory::FFMPEG);
    
    //============================
    //QnResourceDirectoryBrowser
/*    QnResourceDirectoryBrowser::instance().setLocal(true);
    QStringList dirs;
    dirs << qnSettings->mediaFolder();
    dirs << qnSettings->extraMediaFolders();
    QnResourceDirectoryBrowser::instance().setPathCheckList(dirs); */

    /************************************************************************/
    /* Initializing resource searchers                                      */
    /************************************************************************/
/*    QnResourceDiscoveryManager::init(new QnResourceDiscoveryManager());
    m_clientResourceProcessor.moveToThread( QnResourceDiscoveryManager::instance() );
	QnResourceDiscoveryManager::instance()->setResourceProcessor(&m_clientResourceProcessor);

    QnResourceDiscoveryManager::instance()->setReady(true);
    QnResourceDiscoveryManager::instance()->start(); */

//x	qApp->setStyle(qnSkin->newStyle());

	qApp->processEvents();

    return true;
}

void AxHDWitness::doFinalize()
{
    if(m_mainWindow) {
        delete m_mainWindow;
        m_mainWindow = 0;
    }

	QnSessionManager::instance()->stop();
	delete QnCommonModule::instance();
	m_clientModule.reset(0);

    // QnClientMessageProcessor::instance()->stop();
    //xQnSessionManager::instance()->stop();

    // QnResource::stopCommandProc();
    //QnResourceDiscoveryManager::instance().stop();

    qInstallMessageHandler(defaultMsgHandler);

    //xdelete QnResourcePool::instance();
    //xQnResourcePool::initStaticInstance( NULL );
}

void AxHDWitness::createMainWindow() {
    assert(m_mainWindow == NULL);

    m_context.reset(new QnWorkbenchContext(qnResPool));
    m_mainWindow = new QnMainWindow(m_context.data());
    m_mainWindow->setOptions(m_mainWindow->options() & ~(QnMainWindow::TitleBarDraggable | QnMainWindow::WindowButtonsVisible));

    m_mainWindow->setMinimumSize(320, 240);

#ifdef _DEBUG
    /* Show FPS in debug. */
    m_context->menu()->trigger(Qn::ShowFpsAction);
#endif
}
