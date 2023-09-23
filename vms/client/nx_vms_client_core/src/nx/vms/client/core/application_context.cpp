// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlEngine>

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
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/external_resources.h>
#include <utils/media/voice_spectrum_analyzer.h>

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
        if (!nx::vms::common::ServerCompatibilityValidator::isInitialized())
        {
            nx::vms::common::ServerCompatibilityValidator::initialize(
                q->localPeerType());
        }
        cloudStatusWatcher = std::make_unique<CloudStatusWatcher>();
        moduleDiscoveryManager = std::make_unique<nx::vms::discovery::Manager>();
        systemsFinder = std::make_unique<QnSystemsFinder>();
    }

    ApplicationContext* const q;
    const ApplicationContext::Mode mode;
    QQmlEngine* qmlEngine = nullptr;
    std::unique_ptr<Settings> settings;
    std::unique_ptr<CloudStatusWatcher> cloudStatusWatcher;
    std::unique_ptr<QnSystemsFinder> systemsFinder;
    std::unique_ptr<nx::vms::discovery::Manager> moduleDiscoveryManager;
    std::unique_ptr<QnVoiceSpectrumAnalyzer> voiceSpectrumAnalyzer;
    std::unique_ptr<ColorTheme> colorTheme;
    std::unique_ptr<LocalNetworkInterfacesManager> localNetworkInterfacesManager;
    std::unique_ptr<watchers::KnownServerConnections> knownServerConnectionsWatcher;
};

ApplicationContext::ApplicationContext(
    Mode mode,
    PeerType peerType,
    const QString& customCloudHost,
    const QString& customExternalResourceFile,
    QObject* parent)
    :
    common::ApplicationContext(peerType, customCloudHost, parent),
    d(new Private{.q = this, .mode = mode})
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
}

ApplicationContext::~ApplicationContext()
{
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

watchers::KnownServerConnections* ApplicationContext::knownServerConnectionsWatcher() const
{
    return d->knownServerConnectionsWatcher.get();
}

} // namespace nx::vms::client::core
