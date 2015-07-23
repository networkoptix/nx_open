#include "AxHDWitness.h"

//#include <objsafe.h>
//#include <fstream>

//#include <QAxAggregated>
#include <QAxFactory>

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtWidgets/QDesktopWidget>

#include "api/session_manager.h"
#include "core/resource_management/resource_discovery_manager.h"


#include "api/runtime_info_manager.h"
#include "api/app_server_connection.h"
#include <nx_ec/ec2_lib.h>

#include "common/common_module.h"

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>

#include "ui/workbench/workbench_navigator.h"
#include "ui/workbench/workbench_context.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_layout.h"
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

#include "core/resource/camera_history.h"
#include "core/resource/layout_resource.h"
#include "core/resource/media_resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/storage_resource.h"
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include "core/resource/resource_directory_browser.h"
#include <core/resource/storage_plugin_factory.h>

#include "ui/widgets/main_window.h"
#include "client/client_settings.h"

#include "decoders/video/ipp_h264_decoder.h"

#include "utils/network/module_finder.h"
#include "utils/network/router.h"
#include <utils/common/command_line_parser.h>
#include <utils/common/app_info.h>
#include <utils/common/timermanager.h>

#include "utils/server_interface_watcher.h"
#include "ui/actions/action_manager.h"
#include "ui/style/skin.h"
#include "ui/style/globals.h"
#include "decoders/video/abstractdecoder.h"
#include "libavformat/avio.h"
#include "utils/common/util.h"

#include "tests/auto_tester.h"
#include "utils/network/socket.h"
#include <openssl/evp.h>

#include <plugins/resource/server_camera/server_camera_factory.h>
#include "plugins/storage/file_storage/qtfile_storage_resource.h"

#include <utils/common/log.h>
#include <utils/common/synctime.h>


#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench_display.h"
#include <ui/graphics/items/controls/time_slider.h>

#include "ui/workaround/fglrx_full_screen.h"
#include <QtCore/private/qthread_p.h>

#include <QXmlStreamWriter>

#ifdef _DEBUG
    #define QN_WAIT_FOR_DEBUGGER
#endif

static QtMessageHandler defaultMsgHandler = 0;

namespace {

    /* After opening layout, set timeline window to 5 minutes. */
    const qint64 displayWindowLengthMs = 5 * 60 * 1000;

}

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

            QTimer::singleShot(1, m_mainWindow, SLOT(showFullScreen()));
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

double AxHDWitness::minimalSpeed() {
    if(!m_mainWindow)
        return 0.0;
        
    return m_context->navigator()->minimalSpeed();
}

double AxHDWitness::maximalSpeed() {
    if(!m_mainWindow)
        return 0.0;

    return m_context->navigator()->maximalSpeed();
}

double AxHDWitness::speed()
{
    if(!m_mainWindow)
        return 1.0;

    return m_context->navigator()->speed();
}

void AxHDWitness::setSpeed(double speed)
{
    if(!m_mainWindow)
        return;

    qDebug() << "set speed to" << speed;
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

void AxHDWitness::addResourceToLayout(const QString &id, const QString &timestamp) {
    addResourcesToLayout(id, timestamp);
}

void AxHDWitness::addResourcesToLayout(const QString &ids, const QString &timestamp) {
    if (!m_context->user())
        return;

    QnResourceList resources;
    foreach(QString id, ids.split(QLatin1Char('|'))) {
        QnResourcePtr resource = m_context->resourcePool()->getResourceById(id);
        if(resource)
            resources << resource;
    } 

    if (resources.isEmpty())
        return;

    qint64 maxTime = qnSyncTime->currentMSecsSinceEpoch();
    qint64 timeStampMs = timestamp.toLongLong();
    if (timeStampMs < 0)
        timeStampMs = maxTime; /* Live */

    QnTimePeriod period(timeStampMs - displayWindowLengthMs/2, displayWindowLengthMs);
    if (period.endTimeMs() > maxTime) {
        period.startTimeMs = maxTime - displayWindowLengthMs;
        period.durationMs = displayWindowLengthMs;
    }

    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    layout->setId(QnUuid::createUuid());
    layout->setParentId(m_context->user()->getId());
    layout->setCellSpacing(0, 0);
    layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState(true, timeStampMs, 1.0)));
    qnResPool->addResource(layout);  

    QnWorkbenchLayout *wlayout = new QnWorkbenchLayout(layout, this);
    m_context->workbench()->addLayout(wlayout);
    m_context->workbench()->setCurrentLayout(wlayout);
    
    m_context->menu()->trigger(Qn::OpenInCurrentLayoutAction, QnActionParameters(resources).withArgument(Qn::ItemTimeRole, timeStampMs));

    for (QnWorkbenchItem *item: wlayout->items())
        item->setData(Qn::ItemSliderWindowRole, qVariantFromValue(period));

    auto timeSlider = m_context->navigator()->timeSlider();
    /* Disable unused options to make sure our window will be set to fixed size. */
    timeSlider->setOption(QnTimeSlider::StickToMinimum, false);
    timeSlider->setOption(QnTimeSlider::StickToMaximum, false);

    /* Set range to maximum allowed value, so we will not constrain window by any values. */
    timeSlider->setRange(0, maxTime);
    timeSlider->setWindow(period.startTimeMs, period.endTimeMs(), false);
}

void AxHDWitness::removeFromCurrentLayout(const QString &uniqueId) {
    auto layout = m_context->workbench()->currentLayout()->resource();
    if (layout)
        layout->removeItem(layout->getItem(uniqueId));
}

QString AxHDWitness::resourceListXml() {
    QString result;
    
    QXmlStreamWriter writer(&result);
    writer.writeStartDocument();
    writer.writeStartElement(QLatin1String("resources"));

    for(const QnResourcePtr &resource: m_context->resourcePool()->getResources()) {
        QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
        if(!mediaResource)
            continue;

        if (resource->hasFlags(Qn::desktop_camera))
            continue;

        writer.writeStartElement(QLatin1String("resource"));
        writer.writeAttribute(QLatin1String("uniqueId"), resource->getUniqueId());
        writer.writeAttribute(QLatin1String("isCamera"), QString::number(resource->hasFlags(Qn::live_cam)));
        writer.writeAttribute(QLatin1String("name"), resource->getName());
        if(QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>())
            writer.writeAttribute(QLatin1String("ipAddress"), networkResource->getHostAddress());
        writer.writeEndElement();
    }

    writer.writeEndElement();

    return result;
}

void AxHDWitness::reconnect(const QString &url) {

    connect(m_context->action(Qn::ExitAction), &QAction::triggered, this, [this] {
        emit connectionProcessed(1, lit("error"));
    }, Qt::QueuedConnection);

    m_context->menu()->trigger(Qn::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, url) );
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
    
    AllowSetForegroundWindow(ASFW_ANY);

    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths( pluginDirs );

    m_clientModule.reset(new QnClientModule());

    qnSettings->setLightMode(Qn::LightModeActiveX);
    qnRuntime->setActiveXMode(true);

    QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin ? lit(":/skin_light") : lit(":/skin_dark");
    skin.reset(new QnSkin(QStringList() << lit(":/skin") << customizationPath));

    QnCustomization customization;
    customization.add(QnCustomization(skin->path("customization_common.json")));
    customization.add(QnCustomization(skin->path("customization_base.json")));
    customization.add(QnCustomization(skin->path("customization_child.json")));

    customizer.reset(new QnCustomizer(customization));
    customizer->customize(qnGlobals);

    m_ec2ConnectionFactory.reset(getConnectionFactory(Qn::PT_DesktopClient));

    ec2::ResourceContext resCtx(QnServerCameraFactory::instance(), qnResPool, qnResTypePool);
    m_ec2ConnectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( m_ec2ConnectionFactory.data() );
       
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

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);
    
    ffmpegInit();
      
    m_moduleFinder.reset(new QnModuleFinder(true, qnRuntime->isDevMode()));
    m_moduleFinder->start();

    m_router.reset(new QnRouter(m_moduleFinder.data()));

    m_serverInterfaceWatcher.reset(new QnServerInterfaceWatcher(m_router.data()));

    //===========================================================================

    CLVideoDecoderFactory::setCodecManufacture( CLVideoDecoderFactory::AUTO );

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_DesktopClient;
    runtimeData.brand = QnAppInfo::productNameShort();
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo

    auto *style = QnSkin::newStyle();
    QApplication::setStyle(style);
    QApplication::processEvents();

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this, [this] {
        emit connectionProcessed(0, QString());
    }, Qt::QueuedConnection);
    
    return true;
}

void AxHDWitness::doFinalize()
{
    QApplication::processEvents();

    if (m_context)
        disconnect(m_context->navigator(), NULL, this, NULL);

    if (m_mainWindow) {
        delete m_mainWindow;
        m_mainWindow = 0;
    }

    m_ec2ConnectionFactory.reset(NULL);   
    m_context.reset(NULL);

    m_serverInterfaceWatcher.reset(NULL);
    m_router.reset(NULL);
    m_moduleFinder.reset(NULL);

    qInstallMessageHandler(defaultMsgHandler);
    customizer.reset(NULL);

    skin.reset(NULL);

    m_clientModule.reset(NULL);
}

void AxHDWitness::createMainWindow() {
    assert(m_mainWindow == NULL);

    m_context.reset(new QnWorkbenchContext(qnResPool));
    m_context->instance<QnFglrxFullScreen>();

    //TODO: #GDM is it really needed here?
    Qn::ActionId effectiveMaximizeActionId = Qn::FullscreenAction;
    m_context->menu()->registerAlias(Qn::EffectiveMaximizeAction, effectiveMaximizeActionId);

    m_mainWindow = new QnMainWindow(m_context.data());
    m_mainWindow->setOptions(m_mainWindow->options() & ~(QnMainWindow::TitleBarDraggable | QnMainWindow::WindowButtonsVisible));

    m_mainWindow->resize(100, 100);
    m_context->setMainWindow(m_mainWindow);

#ifdef _DEBUG
    /* Show FPS in debug. */
    m_context->menu()->trigger(Qn::ShowFpsAction);
#endif

#ifdef QN_WAIT_FOR_DEBUGGER
    QMessageBox::information(m_mainWindow, "Waiting...", "Waiting for debugger to be attached.");
#endif

}
