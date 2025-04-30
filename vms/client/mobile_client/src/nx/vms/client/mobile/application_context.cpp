// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QFileSelector>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <core/resource/mobile_client_resource_factory.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/build_info.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_async_client.h>
#include <nx/vms/client/core/media/watermark_image_provider.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/mobile_client_meta_types.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/ui/detail/credentials_helper.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/time/formatter.h>
#include <settings/qml_settings_adaptor.h>
#include <utils/mobile_app_info.h>

namespace nx::vms::client::mobile {

namespace {

core::ApplicationContext::Features makeFeatures()
{
    auto result = core::ApplicationContext::Features::all();
    result.ignoreCustomization = qnSettings->ignoreCustomization();
    return result;
}

} // namespace

struct ApplicationContext::Private
{
    ApplicationContext* const q;
    std::unique_ptr<QnMobileClientResourceFactory> resourceFactory =
        std::make_unique<QnMobileClientResourceFactory>();
    std::unique_ptr<nx::client::mobile::QmlSettingsAdaptor> settingsAdaptor;
    std::unique_ptr<WindowContext> windowContext;
    std::unique_ptr<QnMobileAppInfo> appInfo = std::make_unique<QnMobileAppInfo>();
    std::unique_ptr<PushNotificationManager> pushManager;
    std::unique_ptr<detail::CredentialsHelper> credentialsHelper;

    void initializeHolePunching();
    void initializeTranslations();
    void initializePushManager();
    void initializeEngine(const QnMobileClientStartupParameters& params);
    void initializeMainWindowContext();
    void initializeTrafficLogginOptionHandling();
    void initOsSpecificStuff();
};

void ApplicationContext::Private::initializeHolePunching()
{
    if (!qnSettings->enableHolePunching()) {
        // Disable UDP hole punching.
        network::SocketGlobals::instance().cloud().applyArguments(
            ArgumentParser({"--cloud-connect-disable-udp"}));
    }
    else
    {
        NX_DEBUG(this, "Hole punching is enabled");
    }
}

void ApplicationContext::Private::initializeTranslations()
{
    const auto selectedLocale = q->coreSettings()->locale();
    q->initializeTranslations(selectedLocale.isEmpty()
        ? QLocale::system().name()
        : selectedLocale);
}

void ApplicationContext::Private::initializePushManager()
{
    pushManager = std::make_unique<PushNotificationManager>();

    const auto cloudWatcher = q->cloudStatusWatcher();
    const auto updateFirebaseCredentials = utils::guarded(q,
        [this, cloudWatcher]()
        {
            pushManager->setCredentials(
                cloudWatcher->status() == core::CloudStatusWatcher::LoggedOut
                ? network::http::Credentials()
                : cloudWatcher->credentials());
        });

    updateFirebaseCredentials();
    connect(cloudWatcher, &core::CloudStatusWatcher::credentialsChanged,
        q, updateFirebaseCredentials);
    connect(cloudWatcher, &core::CloudStatusWatcher::statusChanged, q,
        [updateFirebaseCredentials](core::CloudStatusWatcher::Status)
        {
            updateFirebaseCredentials();
        });

    const auto updateSourceSystemsForPushNotifications = utils::guarded(q,
        [this, cloudWatcher]()
        {
            QStringList systems;
            for (const auto& system: cloudWatcher->recentCloudSystems())
                systems.append(system.cloudId);
            pushManager->setSystems(systems);
        });
    updateSourceSystemsForPushNotifications();
    connect(cloudWatcher, &core::CloudStatusWatcher::recentCloudSystemsChanged,
        q, updateSourceSystemsForPushNotifications);
}

void ApplicationContext::Private::initializeEngine(
    const QnMobileClientStartupParameters& params)
{
    const auto engine = q->qmlEngine();

    QStringList selectors;
    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);

    QQmlFileSelector qmlFileSelector(engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine->addImageProvider(core::WatermarkImageProvider::name(),
        new core::WatermarkImageProvider());

    static const QString kQmlRoot = "qrc:///qml";
    auto qmlRoot = params.qmlRoot.isEmpty() ? kQmlRoot : params.qmlRoot;
    if (!qmlRoot.endsWith('/'))
        qmlRoot.append('/');
    NX_INFO(this, "Setting QML root to %1", qmlRoot);

    engine->setBaseUrl(
        qmlRoot.startsWith("qrc:") ? QUrl(qmlRoot) : QUrl::fromLocalFile(qmlRoot));
    engine->addImportPath(qmlRoot);

    if (build_info::isIos())
        engine->addImportPath("qt_qml");

    for (const QString& path: params.qmlImportPaths)
        engine->addImportPath(path);
}

void ApplicationContext::Private::initializeMainWindowContext()
{
    settingsAdaptor = std::make_unique<nx::client::mobile::QmlSettingsAdaptor>();
    windowContext = std::make_unique<WindowContext>(q);
}

void ApplicationContext::Private::initializeTrafficLogginOptionHandling()
{
    using namespace nx::network::http;
    const auto updateTrafficLogging =
        []() { AsyncClient::setForceTrafficLogging(qnSettings->forceTrafficLogging()); };

    connect(qnSettings, &QnMobileClientSettings::valueChanged, q,
        [updateTrafficLogging](int id)
        {
            if (id == QnMobileClientSettings::ForceTrafficLogging)
                updateTrafficLogging();
        });

    updateTrafficLogging();
}

void ApplicationContext::Private::initOsSpecificStuff()
{
    if (build_info::isAndroid())
    {
        // We have to use android-specific code to check if we use 24-hours time format.
        time::Formatter::forceSystemTimeFormat(time::is24HoursTimeFormat());
    }
}

//-------------------------------------------------------------------------------------------------

ApplicationContext::ApplicationContext(
    const QnMobileClientStartupParameters& startupParams,
    QObject* parent)
    :
    base_type(Mode::mobileClient,
        Qn::SerializationFormat::json,
        api::PeerType::mobileClient,
        makeFeatures(),
        qnSettings->customCloudHost(),
        /*customExternalResourceFile*/ "mobile_client_external.dat",
        parent),
    d(new Private{
        .q = this,
        .credentialsHelper = std::make_unique<detail::CredentialsHelper>()})
{
    d->initializeHolePunching();
    initializeNetworkModules();
    initializeMetaTypes();
    d->initializeTranslations();
    d->initializeEngine(startupParams);
    d->initializePushManager();
    d->initializeMainWindowContext();
    d->initializeTrafficLogginOptionHandling();
    d->initOsSpecificStuff();
}

ApplicationContext::~ApplicationContext()
{
    resetEngine(); // Frees all systems contexts of the QML objects.

    // DesktopCameraConnection is a SystemContextAware object, so it should be deleted before the
    // SystemContext.
    common::ApplicationContext::stopAll();
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    return mainWindowContext()->system()->as<SystemContext>();
}

WindowContext* ApplicationContext::mainWindowContext() const
{
    return d->windowContext.get();
}

void ApplicationContext::closeWindow()
{
    if (!NX_ASSERT(d->windowContext))
        return;

    // Make sure all QML instances are destroyed before window contexts is dissapeared.
    const auto window = d->windowContext->mainWindow();
    window->deleteLater();
    QObject::connect(window, &QObject::destroyed, qApp, &QGuiApplication::quit);
}

QnMobileAppInfo* ApplicationContext::appInfo() const
{
    return d->appInfo.get();
}

PushNotificationManager* ApplicationContext::pushManager() const
{
    return d->pushManager.get();
}

detail::CredentialsHelper* ApplicationContext::credentialsHelper() const
{
    return d->credentialsHelper.get();
}

nx::client::mobile::QmlSettingsAdaptor* ApplicationContext::settings() const
{
    return d->settingsAdaptor.get();
}

} // namespace nx::vms::client::mobile
