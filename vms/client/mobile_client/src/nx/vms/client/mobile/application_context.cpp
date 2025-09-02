// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <QtCore/QFileSelector>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <core/resource/mobile_client_resource_factory.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/build_info.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/media/watermark_image_provider.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/mobile_client_meta_types.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/ui/detail/credentials_helper.h>
#include <nx/vms/client/mobile/ui/detail/screens.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/time/formatter.h>
#include <settings/qml_settings_adaptor.h>
#include <ui/camera_thumbnail_provider.h>
#include <utils/common/delayed.h>
#include <utils/mobile_app_info.h>

namespace nx::vms::client::mobile {

namespace {

core::ApplicationContext::Features makeFeatures(QnMobileClientSettings* settings)
{
    auto result = core::ApplicationContext::Features::all();
    result.ignoreCustomization = settings->ignoreCustomization();
    return result;
}

} // namespace

struct ApplicationContext::Private
{
    ApplicationContext* const q;
    const nx::Uuid peerId = nx::Uuid::createUuid();
    std::unique_ptr<QnMobileClientSettings> settings;
    std::unique_ptr<QnMobileClientResourceFactory> resourceFactory =
        std::make_unique<QnMobileClientResourceFactory>();
    std::unique_ptr<nx::client::mobile::QmlSettingsAdaptor> settingsAdaptor;
    std::unique_ptr<WindowContext> windowContext;
    std::unique_ptr<SystemContext> mainSystemContext;
    std::unique_ptr<QnMobileAppInfo> appInfo = std::make_unique<QnMobileAppInfo>();
    std::unique_ptr<PushNotificationManager> pushManager;
    std::unique_ptr<detail::CredentialsHelper> credentialsHelper;
    QPointer<QnCameraThumbnailProvider> cameraThumbnailProvider; //< Owned by the QML engine.

    void initializeTranslations();
    void initializePushManager();
    void initializeEngine(const QnMobileClientStartupParameters& params);
    void initializeMainWindowContext();
    void initializeMainSystemContext();
    void initializeTrafficLogginOptionHandling();
    void initOsSpecificStuff();
};

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

    cameraThumbnailProvider = new QnCameraThumbnailProvider();
    engine->addImageProvider("thumbnail", cameraThumbnailProvider);
}

void ApplicationContext::Private::initializeMainWindowContext()
{
    settingsAdaptor = std::make_unique<nx::client::mobile::QmlSettingsAdaptor>();
    windowContext = std::make_unique<WindowContext>(q);
}

void ApplicationContext::Private::initializeMainSystemContext()
{
    mainSystemContext.reset(appContext()->createSystemContext(
        core::SystemContext::Mode::client)->as<SystemContext>());

    q->addSystemContext(mainSystemContext.get());

    connect(mainSystemContext->sessionTimeoutWatcher(),
        &core::RemoteSessionTimeoutWatcher::sessionExpired,
        q,
        nx::utils::guarded(q,
            [this](core::RemoteSessionTimeoutWatcher::SessionExpirationReason)
            {
                const auto manager = appContext()->mainWindowContext()->sessionManager();
                if (!manager)
                    return;

                manager->stopSessionByUser();

                auto error = core::errorDescription(
                    core::RemoteConnectionErrorCode::sessionExpired,
                    /*moduleInformation*/ {}, /*engineVersion*/ {});

                executeLater(
                    [this, error]()
                    {
                        windowContext->uiController()->showMessage(
                            error.shortText, error.longText);
                    }, q);
            }));
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
    std::unique_ptr<QnMobileClientSettings> settings,
    QObject* parent)
    :
    base_type(Mode::mobileClient,
        Qn::SerializationFormat::json,
        api::PeerType::mobileClient,
        makeFeatures(settings.get()),
        settings->customCloudHost(),
        /*customExternalResourceFile*/ "mobile_client_external.dat",
        parent),
    d(new Private{
        .q = this,
        .settings = std::move(settings),
        .credentialsHelper = std::make_unique<detail::CredentialsHelper>()})
{
    initializeNetworkModules(qnSettings->enableHolePunching());
    initializeMetaTypes();
    d->initializeTranslations();
    d->initializeEngine(startupParams);
    d->initializePushManager();
    d->initializeMainWindowContext();
    d->initializeMainSystemContext();
    d->initializeTrafficLogginOptionHandling();
    d->initOsSpecificStuff();
    initializeCrossSystemModules();
    d->windowContext->setMainSystemContext(d->mainSystemContext.get());

    connect(cloudCrossSystemManager(),
        &core::CloudCrossSystemManager::cloudAuthorizationRequested,
        this,
        [this]()
        {
            d->windowContext->uiController()->screens()->showCloudLoginScreen(
                /*reauthentication*/ true);
        });

    connect(qApp, &QGuiApplication::aboutToQuit, this, &ApplicationContext::closeWindow);
}

ApplicationContext::~ApplicationContext()
{
    if (d->mainSystemContext)
        d->mainSystemContext->sessionManager()->resetSession();

    removeSystemContext(d->mainSystemContext.release());

    // Currently, throughout the derived client code (desktop, mobile) it's assumed everywhere
    // that all system contexts are of that most derived level. I.e. desktop client code expects
    // desktop::SystemContext, mobile client code expects mobile::SystemContext everywhere.
    // As soon as some system context creation code appeared at the client::core level,
    // we had to introduce in ApplicationContext a virtual function to create system contexts
    // of the most derived type. However, system contexts often rely on the application context
    // - apparently of the same most derived level. For that reason we must destroy all
    // system contexts in the most derived ApplicationContext destructor.
    destroyCrossSystemModules();

    // System contexts destruction is delayed and delegated to QnLongRunableCleanup.
    // We must enforce it to finish here synchronously.
    common::ApplicationContext::stopAll();
}

core::SystemContext* ApplicationContext::createSystemContext(
    SystemContext::Mode mode, QObject* parent)
{
    return new SystemContext(mainWindowContext(), mode, peerId(), parent);
}

nx::Uuid ApplicationContext::peerId() const
{
    return d->peerId;
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    return d->mainSystemContext.get();
}

WindowContext* ApplicationContext::mainWindowContext() const
{
    return d->windowContext.get();
}

void ApplicationContext::closeWindow()
{
    if (!NX_ASSERT(d->windowContext))
        return;

    if (const auto window = d->windowContext->mainWindow())
    {
        // Make sure all QML instances are destroyed before window contexts is disappeared.
        window->deleteLater();
        QObject::connect(window, &QObject::destroyed, qApp, &QGuiApplication::quit);
    }
}

QnMobileAppInfo* ApplicationContext::appInfo() const
{
    return d->appInfo.get();
}

PushNotificationManager* ApplicationContext::pushManager() const
{
    return d->pushManager.get();
}

QnResourceFactory* ApplicationContext::resourceFactory() const
{
    return d->resourceFactory.get();
}

detail::CredentialsHelper* ApplicationContext::credentialsHelper() const
{
    return d->credentialsHelper.get();
}

QnMobileClientSettings* ApplicationContext::clientSettings() const
{
    return d->settings.get();
}

nx::client::mobile::QmlSettingsAdaptor* ApplicationContext::settings() const
{
    return d->settingsAdaptor.get();
}

QnCameraThumbnailProvider* ApplicationContext::cameraThumbnailProvider() const
{
    return d->cameraThumbnailProvider.get();
}

} // namespace nx::vms::client::mobile
