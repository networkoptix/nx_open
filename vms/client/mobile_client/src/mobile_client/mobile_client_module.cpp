// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_module.h"

#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>

#include <camera/camera_bookmarks_manager.h>
#include <context/context.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/mobile_client_resource_factory.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/router.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/client/mobile/software_trigger/event_rules_watcher.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/maintenance/log/collector_api_paths.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/system_logon/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/maintenance/remote_log_manager.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/time/formatter.h>
#include <ui/window_utils.h>
#include "mobile_client_message_processor.h"
#include "mobile_client_settings.h"
#include "mobile_client_startup_parameters.h"

using namespace nx::vms::client;
using namespace nx::vms::client::mobile;

static const QString kQmlRoot = "qrc:///qml";

template<typename BaseType>
BaseType extract(const QSharedPointer<BaseType>& pointer)
{
    return pointer ? *pointer : BaseType();
}

struct QnMobileClientModule::Private
{
    std::unique_ptr<core::SystemsVisibilityManager> systemsVisibilityManager;
    std::unique_ptr<QnMobileClientResourceFactory> resourceFactory;
};

template<> QnMobileClientModule* Singleton<QnMobileClientModule>::s_instance = nullptr;

QnMobileClientModule::QnMobileClientModule(
    const QnMobileClientStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent),
    d(new Private())
{
    Q_INIT_RESOURCE(mobile_client);

    nx::log::LevelSettings logUploadFilter;
    logUploadFilter.primary = nx::log::Level::verbose;
    logUploadFilter.filters.emplace(std::string("nx"), nx::log::Level::verbose);
    logUploadFilter.filters.emplace(std::string("Qn"), nx::log::Level::verbose);

    m_logUploader = std::make_unique<nx::network::maintenance::log::UploaderManager>(
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setHost(nx::branding::cloudHost()).setPath(nx::network::maintenance::log::kLogCollectorPathPrefix),
        logUploadFilter);

    m_remoteLogManager = std::make_unique<RemoteLogManager>();

    /* Init singletons. */

    m_pushManager.reset(new PushNotificationManager());

    if (!qnSettings->enableHolePunching()) {
        // Disable UDP hole punching.
        nx::network::SocketGlobals::instance().cloud().applyArguments(
            nx::ArgumentParser({"--cloud-connect-disable-udp"}));
    }
    else
    {
        NX_DEBUG(this, "Hole punching is enabled");
    }

    d->systemsVisibilityManager = std::make_unique<core::SystemsVisibilityManager>();


    d->resourceFactory = std::make_unique<QnMobileClientResourceFactory>();

    core::appContext()->knownServerConnectionsWatcher()->start();

    auto qmlRoot = startupParameters.qmlRoot.isEmpty() ? kQmlRoot : startupParameters.qmlRoot;
    if (!qmlRoot.endsWith('/'))
        qmlRoot.append('/');
    NX_INFO(this, "Setting QML root to %1", qmlRoot);

    auto qmlEngine = core::appContext()->qmlEngine();
    qmlEngine->setBaseUrl(
        qmlRoot.startsWith("qrc:") ? QUrl(qmlRoot) : QUrl::fromLocalFile(qmlRoot));
    qmlEngine->addImportPath(qmlRoot);

    if (nx::build_info::isIos())
        qmlEngine->addImportPath("qt_qml");

    for (const QString& path: startupParameters.qmlImportPaths)
        qmlEngine->addImportPath(path);

    if (nx::build_info::isAndroid())
    {
        // We have to use android-specific code to check if we use 24-hours time format.
        nx::vms::time::Formatter::forceSystemTimeFormat(is24HoursTimeFormat());
    }

    const auto handleConnected =
        [this]()
        {
            if (const auto systemContext = appContext()->currentSystemContext())
                systemContext->eventRulesWatcher()->handleConnectionChanged();
        };

    using mobile::SessionManager;
    const auto sessionManager = appContext()->mainWindowContext()->sessionManager();
    connect(sessionManager, &SessionManager::sessionStartedSuccessfully,
        this, handleConnected);
    connect(sessionManager, &SessionManager::sessionRestored,
        this, handleConnected);

    initializePushNotifications();
}

QnMobileClientModule::~QnMobileClientModule()
{
    if (auto longRunnablePool = QnLongRunnablePool::instance())
        longRunnablePool->stopAll();

    const auto windowContext = appContext()->mainWindowContext();
    windowContext->sessionManager()->setSuspended(true);
    if (const auto systemContext = windowContext->mainSystemContext())
        systemContext->setSession({});

    qApp->disconnect(this);
}

PushNotificationManager* QnMobileClientModule::pushManager()
{
    return m_pushManager.data();
}

nx::network::maintenance::log::UploaderManager* QnMobileClientModule::remoteLogsUploader()
{
    return m_logUploader.get();
}

void QnMobileClientModule::initializePushNotifications()
{
    using nx::vms::client::core::CloudStatusWatcher;

    const auto cloudWatcher = core::appContext()->cloudStatusWatcher();
    const auto updateFirebaseCredentials = nx::utils::guarded(this,
        [this, cloudWatcher]()
        {
            m_pushManager->setCredentials(cloudWatcher->status() == CloudStatusWatcher::LoggedOut
                ? nx::network::http::Credentials()
                : cloudWatcher->credentials());
        });

    updateFirebaseCredentials();
    connect(cloudWatcher, &CloudStatusWatcher::credentialsChanged,
        this, updateFirebaseCredentials);
    connect(cloudWatcher, &CloudStatusWatcher::statusChanged, this,
        [updateFirebaseCredentials](CloudStatusWatcher::Status) { updateFirebaseCredentials(); });

    const auto updateSourceSystemsForPushNotifications = nx::utils::guarded(this,
        [this, cloudWatcher]()
        {
            QStringList systems;
            for (const auto& system: cloudWatcher->recentCloudSystems())
                 systems.append(system.cloudId);
            m_pushManager->setSystems(systems);
        });
    updateSourceSystemsForPushNotifications();
    connect(cloudWatcher, &CloudStatusWatcher::recentCloudSystemsChanged,
        this, updateSourceSystemsForPushNotifications);
//    connect(m_pushManager.data(), &PushNotificationManager::showPushErrorMessage,
//        m_context.data(), &QnContext::showMessage);
}
