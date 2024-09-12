// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/private/qv4engine_p.h>

#include <client_core/client_core_meta_types.h>
#include <common/static_common_module.h>
#include <nx/branding.h>
#include <nx/branding_proxy.h>
#include <nx/build_info_proxy.h>
#include <nx/utils/external_resources.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/common/utils/private/thread_pool_manager_p.h>
#include <nx/vms/client/core/event_search/models/visible_item_data_decorator_model.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/font_config.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
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

        common::appContext()->setLocale(settings->locale());
    }

    void initializeQmlEngine()
    {
        auto brandingQmlProxy = new nx::branding::QmlProxy(q);
        qmlRegisterSingletonType<nx::branding::QmlProxy>("nx.vms.client.core", 1, 0, "Branding",
            [brandingQmlProxy](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
            {
                return withCppOwnership(brandingQmlProxy);
            });

        auto buildInfoQmlProxy = new nx::build_info::QmlProxy(q);
        qmlRegisterSingletonType<nx::build_info::QmlProxy>("nx.vms.client.core", 1, 0, "BuildInfo",
            [buildInfoQmlProxy](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
            {
                return withCppOwnership(buildInfoQmlProxy);
            });

        qmlEngine.reset(new QQmlEngine());

        const auto thumbnailProvider = new ThumbnailImageProvider();
        // QQmlEngine takes ownership of thumbnailProvider.
        qmlEngine->addImageProvider(ThumbnailImageProvider::id, thumbnailProvider);
        qmlEngine->addImageProvider(core::VisibleItemDataDecoratorModel::PreviewProvider::id,
            new core::VisibleItemDataDecoratorModel::PreviewProvider());
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
        systemFinder = std::make_unique<SystemFinder>();
    }

    void initializeExternalResources()
    {
        if (!customExternalResourceFile.isEmpty())
            nx::utils::registerExternalResource(customExternalResourceFile);
        nx::utils::registerExternalResource("client_core_external.dat");
        nx::utils::registerExternalResource("bytedance_iconpark.dat",
            analytics::IconManager::librariesRoot() + "bytedance.iconpark/");
    }

    void uninitializeExternalResources()
    {
        if (!customExternalResourceFile.isEmpty())
            nx::utils::unregisterExternalResource(customExternalResourceFile);
        nx::utils::unregisterExternalResource("client_core_external.dat");
        nx::utils::unregisterExternalResource("bytedance_iconpark.dat",
            analytics::IconManager::librariesRoot() + "bytedance.iconpark/");
    }

    ApplicationContext* const q;
    const ApplicationContext::Mode mode;
    const bool ignoreCustomization = false;

    std::unique_ptr<ThreadPool::Manager> threadPoolManager{new ThreadPool::Manager()};
    std::unique_ptr<QQmlEngine> qmlEngine;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<CloudStatusWatcher> cloudStatusWatcher;
    std::unique_ptr<SystemFinder> systemFinder;
    std::unique_ptr<nx::vms::discovery::Manager> moduleDiscoveryManager;
    std::unique_ptr<QnVoiceSpectrumAnalyzer> voiceSpectrumAnalyzer;
    std::unique_ptr<Skin> skin;
    std::unique_ptr<ColorTheme> colorTheme;
    std::vector<QPointer<SystemContext>> systemContexts;
    std::unique_ptr<UnifiedResourcePool> unifiedResourcePool;
    std::unique_ptr<FontConfig> fontConfig;
    std::unique_ptr<LocalNetworkInterfacesManager> localNetworkInterfacesManager;
    std::unique_ptr<watchers::KnownServerConnections> knownServerConnectionsWatcher;
    QString customExternalResourceFile;
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
    d(new Private{
        .q = this,
        .mode = mode,
        .ignoreCustomization = ignoreCustomization,
        .skin = std::make_unique<nx::vms::client::core::Skin>(QStringList{":/skin"}),
        .customExternalResourceFile = customExternalResourceFile
        }
    )
{
    initializeResources();
    d->initializeExternalResources();
    initializeMetaTypes();

    d->initializeSettings();
    d->initializeQmlEngine();

    d->colorTheme = std::make_unique<ColorTheme>();

    switch (mode)
    {
        case Mode::unitTests:
            // For the resources tree tests.
            d->cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();
            break;

        case Mode::desktopClient:
        case Mode::mobileClient:
            d->voiceSpectrumAnalyzer = std::make_unique<QnVoiceSpectrumAnalyzer>();
            d->unifiedResourcePool = std::make_unique<UnifiedResourcePool>();
            d->localNetworkInterfacesManager = std::make_unique<LocalNetworkInterfacesManager>();
            d->knownServerConnectionsWatcher =
                std::make_unique<watchers::KnownServerConnections>();
            break;
    }

    nx::setOnAssertHandler(
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
    d->uninitializeExternalResources();

    nx::setOnAssertHandler(nullptr);
}

void ApplicationContext::initializeNetworkModules()
{
    d->initializeNetworkModules();
}

void ApplicationContext::initializeTranslations(const QString& targetLocale)
{
    NX_INFO(this, "Trying to Initialize translations for locale %1", targetLocale);

    const auto langName = targetLocale.split('_').first();

    std::optional<nx::i18n::Translation> bestTranslation;
    for (const auto& translation: translationManager()->translations())
    {
        if (translation.localeCode == targetLocale)
        {
            bestTranslation = translation;
            break;
        }

        if (translation.localeCode.startsWith(langName))
        {
            bestTranslation = translation;

            // Trying to find the exact translation.
        }
    }

    if (bestTranslation)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Installing translation: %1", bestTranslation->localeCode);
        translationManager()->installTranslation(*bestTranslation);
    }
    else
    {
        NX_DEBUG(NX_SCOPE_TAG, "Installing default translation: %1",
            nx::branding::defaultLocale());
        NX_ASSERT(translationManager()->installTranslation(nx::branding::defaultLocale()),
            "Translations could not be initialized");
    }

    translationManager()->setAssertOnOverlayInstallationFailure();
}

QQmlEngine* ApplicationContext::qmlEngine() const
{
    return d->qmlEngine.get();
}

CloudStatusWatcher* ApplicationContext::cloudStatusWatcher() const
{
    return d->cloudStatusWatcher.get();
}

nx::vms::discovery::Manager* ApplicationContext::moduleDiscoveryManager() const
{
    return d->moduleDiscoveryManager.get();
}

SystemFinder* ApplicationContext::systemFinder() const
{
    return d->systemFinder.get();
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

ThreadPool* ApplicationContext::threadPool(const QString& id) const
{
    return d->threadPoolManager->get(id);
}

FontConfig* ApplicationContext::fontConfig() const
{
    return d->fontConfig.get();
}

void ApplicationContext::resetEngine()
{
    d->qmlEngine->rootContext()->setContextObject(nullptr);
    d->qmlEngine.reset();
}

watchers::KnownServerConnections* ApplicationContext::knownServerConnectionsWatcher() const
{
    return d->knownServerConnectionsWatcher.get();
}

Skin* ApplicationContext::skin() const
{
    return d->skin.get();
}

ColorTheme* ApplicationContext::colorTheme() const
{
    return d->colorTheme.get();
}

UnifiedResourcePool* ApplicationContext::unifiedResourcePool() const
{
    return d->unifiedResourcePool.get();
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    if (NX_ASSERT(!d->systemContexts.empty()))
        return d->systemContexts.front();

    return nullptr;
}

std::vector<SystemContext*> ApplicationContext::systemContexts() const
{
    std::vector<SystemContext*> result;
    for (auto context: d->systemContexts)
    {
        if (NX_ASSERT(context))
            result.push_back(context.data());
    }
    return result;
}

void ApplicationContext::addSystemContext(SystemContext* systemContext)
{
    d->systemContexts.push_back(systemContext);
    emit systemContextAdded(systemContext);
}

void ApplicationContext::removeSystemContext(SystemContext* systemContext)
{
    auto iter = std::find(d->systemContexts.begin(), d->systemContexts.end(), systemContext);
    if (NX_ASSERT(iter != d->systemContexts.end()))
        d->systemContexts.erase(iter);
    emit systemContextRemoved(systemContext);
}

void ApplicationContext::addMainContext(SystemContext* mainContext)
{
    NX_ASSERT(d->systemContexts.empty()); //< Main context should be first.
    d->systemContexts.push_back(mainContext);
}

} // namespace nx::vms::client::core
