// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlEngine>
#include <QtQml/private/qv4engine_p.h>

#include <client_core/client_core_meta_types.h>
#include <common/static_common_module.h>
#include <finders/systems_finder.h>
#include <nx/branding_proxy.h>
#include <nx/build_info_proxy.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/font_config.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/external_resources.h>
#include <utils/media/voice_spectrum_analyzer.h>

extern "C" {

/** QtQml exported helper function for getting QML stacktrace (up to 20 frames) in GDBMI format. */
char* qt_v4StackTraceForEngine(void* executionEngine);

/*
 * Helper and "C" linkage exported function to get the most recent QML stacktrace in GDBMI format.
 * Sample GDB invocation: print nx_qmlStackTrace()
 * Sample CDB invocation: .call nx_vms_client_core!nx_qmlStackTrace() ; gh
 */
NX_FORCE_EXPORT char* nx_qmlStackTrace()
{
    QV4::ExecutionEngine* engine = nx::vms::client::core::appContext()->qmlEngine()->handle();
    if (!engine->qmlContext()) //< Avoid a crash when called without QML stack.
        return nullptr;
    return qt_v4StackTraceForEngine(engine);
}

} // extern "C"

// Resources initialization must be located outside of the namespace.
static void initializeResources()
{
    Q_INIT_RESOURCE(nx_vms_client_core);
}

namespace nx::vms::client::core {

void initializeExternalResources(const QString& customExternalResourceFile)
{
    if (!customExternalResourceFile.isEmpty())
        nx::vms::utils::registerExternalResource(customExternalResourceFile);
    nx::vms::utils::registerExternalResource("client_core_external.dat");
    nx::vms::utils::registerExternalResource("bytedance_iconpark.dat",
        analytics::IconManager::librariesRoot() + "bytedance.iconpark/");
}

static ApplicationContext* s_instance = nullptr;

struct ApplicationContext::Private
{
    void initializeSettings()
    {
        // Make sure application is initialized correctly for settings to be loaded.
        NX_ASSERT(!QCoreApplication::applicationName().isEmpty());
        NX_ASSERT(!QCoreApplication::organizationName().isEmpty());

        // Disable secure settings in unit tests.
        settings = std::make_unique<Settings>(
            Settings::InitializationOptions{.useKeychain = (mode != Mode::unitTests)});
    }

    void initializeQmlEngine()
    {
        auto brandingQmlProxy = new nx::branding::QmlProxy(q);
        qmlRegisterSingletonType<nx::branding::QmlProxy>("nx.vms.client.core", 1, 0, "Branding",
            [brandingQmlProxy](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
            {
                qmlEngine->setObjectOwnership(brandingQmlProxy, QQmlEngine::CppOwnership);
                return brandingQmlProxy;
            });

        auto buildInfoQmlProxy = new nx::build_info::QmlProxy(q);
        qmlRegisterSingletonType<nx::build_info::QmlProxy>("nx.vms.client.core", 1, 0, "BuildInfo",
            [buildInfoQmlProxy](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
            {
                qmlEngine->setObjectOwnership(buildInfoQmlProxy, QQmlEngine::CppOwnership);
                return buildInfoQmlProxy;
            });

        qmlEngine = new QQmlEngine(q);

        const auto thumbnailProvider = new ThumbnailImageProvider();
        // QQmlEngine takes ownership of thumbnailProvider.
        qmlEngine->addImageProvider(ThumbnailImageProvider::id, thumbnailProvider);
    }

    void initializeNetworkModules()
    {
        using namespace nx::vms::common;
        using Protocol = ServerCompatibilityValidator::Protocol;

        ServerCompatibilityValidator::DeveloperFlags developerFlags;
        if (ignoreCustomization)
            developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCustomization);

        if (!nx::vms::common::ServerCompatibilityValidator::isInitialized())
        {
            nx::vms::common::ServerCompatibilityValidator::initialize(
                q->localPeerType(),
                Protocol::autoDetect,
                developerFlags);
        }
        cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();
        moduleDiscoveryManager = std::make_unique<nx::vms::discovery::Manager>();
        systemsFinder = std::make_unique<QnSystemsFinder>();
    }

    ApplicationContext* const q;
    const ApplicationContext::Mode mode;
    const bool ignoreCustomization = false;

    QQmlEngine* qmlEngine = nullptr;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<CloudStatusWatcher> cloudStatusWatcher;
    std::unique_ptr<QnSystemsFinder> systemsFinder;
    std::unique_ptr<nx::vms::discovery::Manager> moduleDiscoveryManager;
    std::unique_ptr<QnVoiceSpectrumAnalyzer> voiceSpectrumAnalyzer;
    std::unique_ptr<ColorTheme> colorTheme;
    std::unique_ptr<FontConfig> fontConfig;
    std::unique_ptr<LocalNetworkInterfacesManager> localNetworkInterfacesManager;
    std::unique_ptr<watchers::KnownServerConnections> knownServerConnectionsWatcher;
};

ApplicationContext::ApplicationContext(
    Mode mode,
    PeerType peerType,
    const QString& customCloudHost,
    bool ignoreCustomization,
    const QString& customExternalResourceFile,
    QObject* parent)
    :
    common::ApplicationContext(peerType, customCloudHost, parent),
    d(new Private{.q = this, .mode = mode, .ignoreCustomization = ignoreCustomization})
{
    if (NX_ASSERT(!s_instance))
        s_instance = this;

    initializeResources();
    initializeExternalResources(customExternalResourceFile);
    initializeMetaTypes();

    d->initializeSettings();
    d->initializeQmlEngine();

    switch (mode)
    {
        case Mode::unitTests:
            // For the resources tree tests.
            d->cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();
            break;

        case Mode::desktopClient:
        case Mode::mobileClient:
            d->voiceSpectrumAnalyzer = std::make_unique<QnVoiceSpectrumAnalyzer>();
            d->colorTheme = std::make_unique<ColorTheme>();
            d->localNetworkInterfacesManager = std::make_unique<LocalNetworkInterfacesManager>();
            d->knownServerConnectionsWatcher =
                std::make_unique<watchers::KnownServerConnections>();
            break;
    }

    nx::utils::setOnAssertHandler(
        [](const QString&)
        {
            std::cerr << "\nQML stacktrace:\n";

            const QV4::ExecutionEngine* engine = appContext()->qmlEngine()->handle();
            if (!engine->qmlContext()) //< Avoid a crash when called without QML stack.
            {
                std::cerr << "  <no QML context available>" << std::endl;
                return;
            }

            const QVector<QV4::StackFrame> stackTrace = engine->stackTrace(20);
            for (int i = 0; i < stackTrace.size(); ++i)
            {
                const QUrl url(stackTrace.at(i).source);
                const QString fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();

                std::cerr << nx::format("  #%1 %2 in %3:%4\n",
                    i, stackTrace.at(i).function, fileName, stackTrace.at(i).line).toStdString();
            }

            std::cerr << std::endl;
        });
}

ApplicationContext::~ApplicationContext()
{
    nx::utils::setOnAssertHandler(nullptr);

    if (NX_ASSERT(s_instance == this))
        s_instance = nullptr;
}

ApplicationContext* ApplicationContext::instance()
{
    NX_ASSERT(s_instance);
    return s_instance;
}

void ApplicationContext::initializeNetworkModules()
{
    d->initializeNetworkModules();
}

QQmlEngine* ApplicationContext::qmlEngine() const
{
    return d->qmlEngine;
}

CloudStatusWatcher* ApplicationContext::cloudStatusWatcher() const
{
    return d->cloudStatusWatcher.get();
}

nx::vms::discovery::Manager* ApplicationContext::moduleDiscoveryManager() const
{
    return d->moduleDiscoveryManager.get();
}

QnSystemsFinder* ApplicationContext::systemsFinder() const
{
    return d->systemsFinder.get();
}

QnVoiceSpectrumAnalyzer* ApplicationContext::voiceSpectrumAnalyzer() const
{
    return d->voiceSpectrumAnalyzer.get();
}

Settings* ApplicationContext::coreSettings() const
{
    return d->settings.get();
}

void ApplicationContext::storeFontConfig(FontConfig* config)
{
    d->fontConfig.reset(config);
}

FontConfig* ApplicationContext::fontConfig() const
{
    return d->fontConfig.get();
}

watchers::KnownServerConnections* ApplicationContext::knownServerConnectionsWatcher() const
{
    return d->knownServerConnectionsWatcher.get();
}

} // namespace nx::vms::client::core
