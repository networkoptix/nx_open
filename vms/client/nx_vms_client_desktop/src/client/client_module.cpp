// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_module.h"

#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QResource>
#include <QtGui/QPalette>
#include <QtGui/QSurfaceFormat>
#include <QtWebEngine/QtWebEngine>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>

#include <api/network_proxy_factory.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/forgotten_systems_manager.h>
#include <client_core/client_core_module.h>
#include <client_core/client_core_settings.h>
#include <common/common_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <decoders/video/abstract_video_decoder.h>
#include <finders/systems_finder.h>
#include <nx/build_info.h>
#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/network/socket_global.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/vms/client/core/resource/screen_recording/audio_only/desktop_audio_only_resource.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource_searcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/analytics/analytics_metadata_provider_factory.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager_factory.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/svg_icon_provider.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/license_health_watcher.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/utils/local_proxy_server.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/time/formatter.h>
#include <ui/workaround/qtbug_workaround.h>
#include <utils/common/command_line_parser.h>
#include <utils/math/color_transformations.h>
#include <watchers/cloud_status_watcher.h>

#if defined(Q_OS_WIN)
    #include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/windows_desktop_resource.h>
    #include <nx/vms/client/desktop/resource/screen_recording/audio_video_win/windows_desktop_resource_searcher_impl.h>
#else
    #include <nx/vms/client/core/resource/screen_recording/audio_only/desktop_audio_only_resource_searcher_impl.h>
#endif
#if defined(Q_OS_MAC)
    #include <ui/workaround/mac_utils.h>
#endif

#include <nx/vms/client/desktop/state/running_instances_manager.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

QPalette makeApplicationPalette()
{
    QPalette result(QApplication::palette());
    result.setColor(QPalette::WindowText, colorTheme()->color("light16"));
    result.setColor(QPalette::Button, colorTheme()->color("dark11"));
    result.setColor(QPalette::Light, colorTheme()->color("light10"));
    result.setColor(QPalette::Midlight, colorTheme()->color("dark13"));
    result.setColor(QPalette::Dark, colorTheme()->color("dark9"));
    result.setColor(QPalette::Mid, colorTheme()->color("dark10"));
    result.setColor(QPalette::Text, colorTheme()->color("light4"));
    result.setColor(QPalette::BrightText, colorTheme()->color("light1"));
    result.setColor(QPalette::ButtonText, colorTheme()->color("light4"));
    result.setColor(QPalette::Base, colorTheme()->color("dark7"));
    result.setColor(QPalette::Window, colorTheme()->color("dark7"));
    result.setColor(QPalette::Shadow, colorTheme()->color("dark5"));
    result.setColor(QPalette::Highlight, colorTheme()->color("brand_core"));
    result.setColor(QPalette::HighlightedText, colorTheme()->color("brand_contrast"));
    result.setColor(QPalette::Link, colorTheme()->color("brand_d2"));
    result.setColor(QPalette::LinkVisited, colorTheme()->color("brand_core"));
    result.setColor(QPalette::AlternateBase, colorTheme()->color("dark7"));
    result.setColor(QPalette::ToolTipBase, colorTheme()->color("light4"));
    result.setColor(QPalette::ToolTipText, colorTheme()->color("dark4"));
    result.setColor(QPalette::PlaceholderText, colorTheme()->color("light16"));

    static const auto kDisabledAlpha = 77;
    static const QList<QPalette::ColorRole> kDimmerRoles{{
        QPalette::WindowText, QPalette::Button, QPalette::Light,
        QPalette::Midlight, QPalette::Dark, QPalette::Mid,
        QPalette::Text, QPalette::BrightText, QPalette::ButtonText,
        QPalette::Base, QPalette::Shadow, QPalette::HighlightedText,
        QPalette::Link, QPalette::LinkVisited, QPalette::AlternateBase}};

    for (const QPalette::ColorRole role: kDimmerRoles)
        result.setColor(QPalette::Disabled, role, withAlpha(result.color(role), kDisabledAlpha));

    return result;
}

} // namespace

template<> QnClientModule* Singleton<QnClientModule>::s_instance = nullptr;

struct QnClientModule::Private
{
    void initLicensesModule()
    {
        using namespace nx::vms::license;

        videoWallLicenseUsageHelper =
            std::make_unique<VideoWallLicenseUsageHelper>(appContext()->currentSystemContext());
        videoWallLicenseUsageHelper->setCustomValidator(
            std::make_unique<license::VideoWallLicenseValidator>(
                appContext()->currentSystemContext()));
    }

    const QnStartupParameters startupParameters;
    std::unique_ptr<AnalyticsSettingsManager> analyticsSettingsManager;
    std::unique_ptr<nx::vms::client::desktop::analytics::AttributeHelper> analyticsAttributeHelper;
    std::unique_ptr<nx::vms::license::VideoWallLicenseUsageHelper> videoWallLicenseUsageHelper;
    std::unique_ptr<DebugInfoStorage> debugInfoStorage;
};

QnClientModule::QnClientModule(const QnStartupParameters& startupParameters, QObject* parent):
    QObject(parent),
    d(new Private{.startupParameters = startupParameters})
{
    // Shortened initialization if run in self-update mode.
    if (d->startupParameters.selfUpdateMode)
        return;

    auto commonModule = clientCoreModule()->commonModule();

    commonModule->store(new QnQtbugWorkaround());

    commonModule->store(new nx::cloud::gateway::VmsGatewayEmbeddable(true));
    commonModule->store(new LocalProxyServer());

    commonModule->store(new SystemInternetAccessWatcher(commonModule));
    commonModule->findInstance<nx::vms::client::core::watchers::KnownServerConnections>()->start();

    d->analyticsSettingsManager = AnalyticsSettingsManagerFactory::createAnalyticsSettingsManager(
        appContext()->currentSystemContext()->resourcePool(),
        appContext()->currentSystemContext()->messageProcessor());

    d->analyticsAttributeHelper = std::make_unique<
        nx::vms::client::desktop::analytics::AttributeHelper>(
            appContext()->currentSystemContext()->analyticsTaxonomyStateWatcher());

    m_analyticsMetadataProviderFactory.reset(new AnalyticsMetadataProviderFactory());
    m_analyticsMetadataProviderFactory->registerMetadataProviders();

    registerResourceDataProviders();
    integrations::initialize(this);

    m_performanceMonitor.reset(new PerformanceMonitor());
    m_licenseHealthWatcher.reset(new LicenseHealthWatcher(
        appContext()->currentSystemContext()->licensePool()));

    d->debugInfoStorage = std::make_unique<DebugInfoStorage>();

    d->initLicensesModule();
    initNetwork();

    // Initialize application UI.
    initSkin();

    initSurfaceFormat();
}

QnClientModule::~QnClientModule()
{
    // Stop all long runnables before deinitializing singletons. Pool may not exist in update mode.
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    // Restoring default message handler.
    nx::utils::disableQtMessageAsserts();
}

void QnClientModule::initDesktopCamera([[maybe_unused]] QOpenGLWidget* window)
{
    /* Initialize desktop camera searcher. */
    auto commonModule = clientCoreModule()->commonModule();
#if defined(Q_OS_WIN)
    auto impl = new WindowsDesktopResourceSearcherImpl(window);
#else
    auto impl = new core::DesktopAudioOnlyResourceSearcherImpl();
#endif
    auto desktopSearcher = commonModule->store(new core::DesktopResourceSearcher(impl));
    desktopSearcher->setLocal(true);
    appContext()->resourceDiscoveryManager()->addDeviceSearcher(desktopSearcher);
}

void QnClientModule::startLocalSearchers()
{
    appContext()->resourceDiscoveryManager()->start();
}

void QnClientModule::initSurfaceFormat()
{
    // Warning: do not set version or profile here.

    auto format = QSurfaceFormat::defaultFormat();

    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoMultisampling))
        format.setSamples(2);
    format.setSwapBehavior(qnSettings->isGlDoubleBuffer()
        ? QSurfaceFormat::DoubleBuffer
        : QSurfaceFormat::SingleBuffer);
    format.setSwapInterval(ini().limitFrameRate ? 1 : 0);

    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    QSurfaceFormat::setDefaultFormat(format);
}

void QnClientModule::initNetwork()
{
    auto commonModule = clientCoreModule()->commonModule();

    appContext()->moduleDiscoveryManager()->start(systemContext()->resourcePool());

    commonModule->store(new QnForgottenSystemsManager());
    commonModule->store(new nx::vms::client::core::SystemsVisibilityManager());
}

void QnClientModule::initSkin()
{
    QStringList paths;
    paths << ":/skin";

    QScopedPointer<Skin> skin(new Skin(paths));

    QApplication::setWindowIcon(qnSkin->icon(":/logo.png"));
    QApplication::setStyle(skin->newStyle());

    auto commonModule = clientCoreModule()->commonModule();
    commonModule->store(skin.take());
    commonModule->store(new ColorTheme());
    commonModule->store(new CustomCursors(Skin::instance()));

    QApplication::setPalette(makeApplicationPalette());
    QToolTip::setPalette(QApplication::palette());

    appContext()->qmlEngine()->addImageProvider("svg", new SvgIconProvider());
}

void QnClientModule::initWebEngine()
{
    // QtWebEngine uses a dedicated process to handle web pages. That process needs to know from
    // where to load libraries. It's not a problem for release packages since everything is
    // gathered in one place, but it is a problem for development builds. The simplest solution for
    // this is to set library search path variable. In Linux this variable is needed only for
    // QtWebEngine::defaultSettings() call which seems to create a WebEngine process. After the
    // variable could be restored to the original value. In macOS it's needed for every web page
    // constructor, so we just set it for the whole lifetime of Client application.

    const QByteArray libraryPathVariable =
        nx::build_info::isLinux()
            ? "LD_LIBRARY_PATH"
            : nx::build_info::isMacOsX()
                ? "DYLD_LIBRARY_PATH"
                : "";

    const QByteArray originalLibraryPath =
        libraryPathVariable.isEmpty() ? QByteArray() : qgetenv(libraryPathVariable);

    if (!libraryPathVariable.isEmpty())
    {
        QString libPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../lib");
        if (!originalLibraryPath.isEmpty())
        {
            libPath += ':';
            libPath += originalLibraryPath;
        }

        qputenv(libraryPathVariable, libPath.toLocal8Bit());
    }

    qputenv("QTWEBENGINE_DIALOG_SET", "QtQuickControls2");

    const auto settings = QWebEngineSettings::defaultSettings();
    // We must support Flash for some camera admin pages to work.
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    // TODO: Add ini parameters for WebEngine attributes
    //settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    //settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    if (!nx::build_info::isMacOsX())
    {
        if (!originalLibraryPath.isEmpty())
            qputenv(libraryPathVariable, originalLibraryPath);
        else
            qunsetenv(libraryPathVariable);
    }
}

QnClientCoreModule* QnClientModule::clientCoreModule() const
{
    return appContext()->clientCoreModule();
}

SystemContext* QnClientModule::systemContext() const
{
    return appContext()->currentSystemContext();
}

QnStartupParameters QnClientModule::startupParameters() const
{
    return d->startupParameters;
}

PerformanceMonitor* QnClientModule::performanceMonitor() const
{
    return m_performanceMonitor.data();
}

nx::vms::license::VideoWallLicenseUsageHelper* QnClientModule::videoWallLicenseUsageHelper() const
{
    return d->videoWallLicenseUsageHelper.get();
}

nx::vms::client::desktop::analytics::TaxonomyManager* QnClientModule::taxonomyManager() const
{
    using TaxonomyManager = nx::vms::client::desktop::analytics::TaxonomyManager;

    return appContext()->qmlEngine()->singletonInstance<TaxonomyManager*>(
        qmlTypeId("nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager"));
}

nx::vms::client::desktop::analytics::AttributeHelper*
    QnClientModule::analyticsAttributeHelper() const
{
    return d->analyticsAttributeHelper.get();
}

AnalyticsSettingsManager* QnClientModule::analyticsSettingsManager() const
{
    return d->analyticsSettingsManager.get();
}

void QnClientModule::registerResourceDataProviders()
{
    auto factory = qnClientCoreModule->dataProviderFactory();
    factory->registerResourceType<QnAviResource>();
    factory->registerResourceType<QnClientCameraResource>();
    #if defined(Q_OS_WIN)
        factory->registerResourceType<WindowsDesktopResource>();
    #endif
}
