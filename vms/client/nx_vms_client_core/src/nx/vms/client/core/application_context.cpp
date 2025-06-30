// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/private/qv4engine_p.h>

#include <client_core/client_core_meta_types.h>
#include <nx/branding.h>
#include <nx/branding_proxy.h>
#include <nx/build_info_proxy.h>
#include <nx/p2p/p2p_ini.h>
#include <nx/utils/external_resources.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/common/utils/private/thread_pool_manager_p.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_layouts_watcher.h>
#include <nx/vms/client/core/event_search/models/visible_item_data_decorator_model.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/session_token_terminator.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/screen_recording/audio_only/desktop_audio_only_data_provider.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/font_config.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/skin_image_provider.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
#include <utils/common/long_runable_cleanup.h>

#include "application_context_qml_initializer.h"

extern "C" {

/** QtQml exported helper function for getting QML stacktrace (up to 20 frames) in GDBMI format. */
char* qt_v4StackTraceForEngine(void* executionEngine);

/*
 * Helper and "C" linkage exported function to get the most recent QML stacktrace in GDBMI format.
 * Sample GDB invocation: print nx_qmlStackTrace()
 * Sample CDB invocation: .call nx_vms_client_core!nx_qmlStackTrace();
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

using CommonFeatureFlag = common::ApplicationContext::FeatureFlag;

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

    void initializeNetworkModule()
    {
        sessionTokenTerminator = std::make_unique<SessionTokenTerminator>();
        networkModule = std::make_unique<NetworkModule>(q);
        const auto settings = q->coreSettings();
        connect(&settings->certificateValidationLevel, &Settings::BaseProperty::changed, q,
            [this](nx::utils::property_storage::BaseProperty* /*property*/)
            {
                networkModule->reinitializeCertificateStorage();
            });
    }

    ApplicationContext* const q;
    const ApplicationContext::Mode mode;
    const Qn::SerializationFormat format;
    const Features features;

    std::unique_ptr<ThreadPool::Manager> threadPoolManager{new ThreadPool::Manager()};
    std::unique_ptr<QQmlEngine> qmlEngine;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<CloudStatusWatcher> cloudStatusWatcher;
    std::unique_ptr<SystemFinder> systemFinder;
    std::unique_ptr<nx::vms::discovery::Manager> moduleDiscoveryManager;
    std::unique_ptr<VoiceSpectrumAnalyzer> voiceSpectrumAnalyzer;
    std::unique_ptr<Skin> skin;
    std::unique_ptr<ColorTheme> colorTheme;
    std::vector<QPointer<SystemContext>> systemContexts;
    std::unique_ptr<UnifiedResourcePool> unifiedResourcePool;
    std::unique_ptr<FontConfig> fontConfig;
    std::unique_ptr<LocalNetworkInterfacesManager> localNetworkInterfacesManager;
    std::unique_ptr<watchers::KnownServerConnections> knownServerConnectionsWatcher;
    std::unique_ptr<SessionTokenTerminator> sessionTokenTerminator;
    std::unique_ptr<NetworkModule> networkModule;
    std::unique_ptr<SystemsVisibilityManager> systemsVisibilityManager;
    std::unique_ptr<CloudCrossSystemManager> cloudCrossSystemManager;
    std::unique_ptr<CloudLayoutsManager> cloudLayoutsManager;
    std::unique_ptr<CrossSystemLayoutsWatcher> crossSystemLayoutsWatcher;

    QString customExternalResourceFile;
};

ApplicationContext::ApplicationContext(
    Mode mode,
    Qn::SerializationFormat serializationFormat,
    PeerType peerType,
    Features features,
    const QString& customCloudHost,
    const QString& customExternalResourceFile,
    QObject* parent)
    :
    common::ApplicationContext(
        peerType,
        features.base,
        customCloudHost,
        parent),
    d(new Private{
        .q = this,
        .mode = mode,
        .format = serializationFormat,
        .features = features,
        .skin = std::make_unique<nx::vms::client::core::Skin>(QStringList{":/skin"}),
        .customExternalResourceFile = customExternalResourceFile
        }
    )
{
    initializeResources();
    d->initializeExternalResources();
    initializeMetaTypes();

    d->initializeSettings();
    d->colorTheme = std::make_unique<ColorTheme>();

    if (features.flags.testFlag(FeatureFlag::qml))
    {
        registerQmlTypes();
        d->initializeQmlEngine();
    }

    switch (mode)
    {
        case Mode::unitTests:
            // For the resources tree tests.
            d->cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();
            break;

        case Mode::desktopClient:
        case Mode::mobileClient:
            d->voiceSpectrumAnalyzer = std::make_unique<VoiceSpectrumAnalyzer>();
            d->unifiedResourcePool = std::make_unique<UnifiedResourcePool>();
            d->localNetworkInterfacesManager = std::make_unique<LocalNetworkInterfacesManager>();
            d->knownServerConnectionsWatcher =
                std::make_unique<watchers::KnownServerConnections>();
            d->systemsVisibilityManager = std::make_unique<core::SystemsVisibilityManager>();
            break;
    }

    if (features.base.flags.testFlag(CommonFeatureFlag::translations))
        translationManager()->startLoadingTranslations();

    if (const auto engine = appContext()->qmlEngine())
    {
        engine->addImageProvider("skin", new nx::vms::client::core::SkinImageProvider());
        engine->addImageProvider("remote", new RemoteAsyncImageProvider());
    }

    if (features.base.flags.testFlag(CommonFeatureFlag::networking))
        d->initializeNetworkModule();

    if (features.flags.testFlag(FeatureFlag::qml))
        ApplicationContextQmlInitializer::storeToQmlContext(this);
}

ApplicationContext::~ApplicationContext()
{
    if (const auto engine = qmlEngine())
        engine->removeImageProvider("skin");

    if (d->features.base.flags.testFlag(CommonFeatureFlag::translations))
        translationManager()->stopLoadingTranslations();
    d->uninitializeExternalResources();

    nx::setOnAssertHandler(nullptr);
}

Qn::SerializationFormat ApplicationContext::serializationFormat() const
{
    return d->format;
}

void ApplicationContext::initializeNetworkModules()
{
    NX_ASSERT(d->features.base.flags.testFlag(CommonFeatureFlag::networking));

    using namespace nx::vms::common;

    ServerCompatibilityValidator::DeveloperFlags developerFlags;
    if (d->features.ignoreCustomization)
        developerFlags.setFlag(ServerCompatibilityValidator::DeveloperFlag::ignoreCustomization);

    if (!nx::vms::common::ServerCompatibilityValidator::isInitialized())
    {
        nx::vms::common::ServerCompatibilityValidator::initialize(
            localPeerType(), d->format, developerFlags);
    }
    d->cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();

    d->moduleDiscoveryManager = std::make_unique<nx::vms::discovery::Manager>();
    moduleDiscoveryManager()->start();

    d->systemFinder = std::make_unique<SystemFinder>();

    if (d->knownServerConnectionsWatcher)
        d->knownServerConnectionsWatcher->start();
}

void ApplicationContext::initializeCrossSystemModules()
{
    NX_ASSERT(d->features.base.flags.testFlag(CommonFeatureFlag::networking));
    NX_ASSERT(d->features.flags.testFlag(FeatureFlag::cross_site));

    // Cross-system cameras should process cloud status changes after cloudLayoutsManager. This
    // makes "Logout from Cloud" scenario much more smooth. If layouts are closed before camera
    // resources are removed, we do not need process widgets one by one.
    d->cloudLayoutsManager = std::make_unique<CloudLayoutsManager>();
    d->cloudCrossSystemManager = std::make_unique<CloudCrossSystemManager>();
    d->crossSystemLayoutsWatcher = std::make_unique<CrossSystemLayoutsWatcher>();
}

void ApplicationContext::destroyCrossSystemModules()
{
    d->crossSystemLayoutsWatcher.reset();
    d->cloudCrossSystemManager.reset();
    d->cloudLayoutsManager.reset();
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

VoiceSpectrumAnalyzer* ApplicationContext::voiceSpectrumAnalyzer() const
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

nx::Uuid ApplicationContext::peerId() const
{
    if (d->mode == Mode::unitTests)
    {
        static const nx::Uuid id = nx::Uuid::createUuid();
        return id;
    }

    return nx::Uuid{};
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

SystemContext* ApplicationContext::createSystemContext(SystemContext::Mode mode, QObject* parent)
{
    return new SystemContext(mode, peerId(), parent);
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
    longRunableCleanup()->clearContextAsync(systemContext);
}

SystemContext* ApplicationContext::systemContextByCloudSystemId(const QString& cloudSystemId) const
{
    if (cloudSystemId == genericCloudSystemId())
        return cloudLayoutsSystemContext();

    if (const auto cloudContext = d->cloudCrossSystemManager->systemContext(cloudSystemId))
        return cloudContext->systemContext();

    return nullptr;
}

CloudCrossSystemManager* ApplicationContext::cloudCrossSystemManager() const
{
    return d->cloudCrossSystemManager.get();
}

CloudLayoutsManager* ApplicationContext::cloudLayoutsManager() const
{
    return d->cloudLayoutsManager.get();
}

SystemContext* ApplicationContext::cloudLayoutsSystemContext() const
{
    if (NX_ASSERT(d->cloudLayoutsManager))
        return d->cloudLayoutsManager->systemContext();

    return {};
}

QnResourcePool* ApplicationContext::cloudLayoutsPool() const
{
    return cloudLayoutsSystemContext()->resourcePool();
}

void ApplicationContext::addMainContext(SystemContext* mainContext)
{
    NX_ASSERT(d->systemContexts.empty()); //< Main context should be first.
    addSystemContext(mainContext);
}

bool ApplicationContext::isCertificateValidationLevelStrict() const
{
    return coreSettings()->certificateValidationLevel()
        == network::server_certificate::ValidationLevel::strict;
}

NetworkModule* ApplicationContext::networkModule() const
{
    return d->networkModule.get();
}

SessionTokenTerminator* ApplicationContext::sessionTokenTerminator() const
{
    return d->sessionTokenTerminator.get();
}

std::unique_ptr<QnAbstractStreamDataProvider> ApplicationContext::createAudioInputProvider() const
{
    return std::make_unique<DesktopAudioOnlyDataProvider>();
}

} // namespace nx::vms::client::core
