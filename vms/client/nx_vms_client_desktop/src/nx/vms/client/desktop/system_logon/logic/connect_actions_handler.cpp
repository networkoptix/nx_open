// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_actions_handler.h"

#include <chrono>

#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QAction>

#include <api/runtime_info_manager.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/desktop_client_message_processor.h>
#include <client_core/client_core_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <finders/systems_finder.h>
#include <licensing/license.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx/analytics/utils.h>
#include <nx/build_info.h>
#include <nx/monitoring/hardware_information.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/app_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/reconnect_helper.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/connecting_to_server_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_license_notifier.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/applauncher_utils.h>
#include <utils/common/delayed.h>
#include <utils/connection_diagnostics_helper.h>

#include "../ui/login_dialog.h"
#include "../ui/welcome_screen.h"
#include "context_current_user_watcher.h"
#include "local_session_token_expiration_watcher.h"
#include "remote_session.h"
#include "temporary_user_expiration_watcher.h"
#include "user_auth_debug_info_watcher.h"

using namespace std::chrono;
using namespace nx::vms::client::core;

namespace {

static const int kMessagesDelayMs = 5000;

static constexpr auto kSelectCurrentServerShowDialogDelay = 250ms;

bool isConnectionToCloud(const LogonData& logonData)
{
    return logonData.userType == nx::vms::api::UserType::cloud;
}

} // namespace

namespace nx::vms::client::desktop {

QString toString(ConnectActionsHandler::LogicalState state)
{
    return QString::fromStdString(nx::reflect::toString(state));
}

struct ConnectActionsHandler::Private
{
    ConnectActionsHandler* const q;
    LogicalState logicalState = LogicalState::disconnected;
    QPointer<QnReconnectInfoDialog> reconnectDialog;
    QPointer<ConnectingToServerDialog> switchServerDialog;
    RemoteConnectionFactory::ProcessPtr currentConnectionProcess;
    /** Flag that we should handle new connection. */
    bool warnMessagesDisplayed = false;
    std::unique_ptr<nx::utils::TimerManager> timerManager;
    std::unique_ptr<ec2::CrashReporter> crashReporter;

    Private(ConnectActionsHandler* owner):
        q(owner)
    {
    }

    RemoteConnectionFactory::Callback makeSingleConnectionCallback(
        RemoteConnectionFactory::Callback callback)
    {
        return nx::utils::guarded(q,
            [this, callback = std::move(callback)]
                (RemoteConnectionFactory::ConnectionOrError result)
            {
                if (currentConnectionProcess)
                    callback(result);
                currentConnectionProcess.reset();
            });
    }

    void handleWSError(RemoteConnectionErrorCode errorCode) const
    {
        const auto welcomeScreen = q->mainWindow()->welcomeScreen();
        const bool connectingTileExists =
            welcomeScreen && welcomeScreen->connectingTileExists();
        if (connectingTileExists)
            welcomeScreen->openConnectingTile(errorCode);
    }
};

ConnectActionsHandler::ConnectActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    d->timerManager = std::make_unique<nx::utils::TimerManager>("CrashReportTimers");
    d->crashReporter = std::make_unique<ec2::CrashReporter>(
        systemContext(),
        d->timerManager.get(),
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/crashReporter");

    // Videowall must not disconnect automatically as we may have not option to restart it.
    // ACS clients display only fixed part of the archive, so they look quite safe.
    if (qnRuntime->isDesktopMode())
    {
        auto sessionTimeoutWatcher = qnClientCoreModule->networkModule()->sessionTimeoutWatcher();
        connect(sessionTimeoutWatcher, &RemoteSessionTimeoutWatcher::sessionExpired, this,
            [this](RemoteSessionTimeoutWatcher::SessionExpirationReason reason)
            {
                const bool isTemporaryUser = connection()->connectionInfo().isTemporary();
                executeDelayedParented(
                    [this, isTemporaryUser, reason]()
                    {
                        // When temporary token is used for authorization, common session token is
                        // created and used instead. This session token can wear off earlier than
                        // the temporary token, but for security reasons we should ask user to
                        // re-enter temporary token manually.
                        using SessionExpirationReason
                            = RemoteSessionTimeoutWatcher::SessionExpirationReason;

                        if (isTemporaryUser && reason == SessionExpirationReason::sessionExpired)
                        {
                            QnConnectionDiagnosticsHelper::showTemporaryUserReAuthMessage(
                                mainWindowWidget());
                        }
                        else
                        {
                            auto errorCode =
                                reason == SessionExpirationReason::temporaryTokenExpired
                                    ? RemoteConnectionErrorCode::temporaryTokenExpired
                                    : RemoteConnectionErrorCode::sessionExpired;

                            d->handleWSError(errorCode);

                            QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                                context(),
                                errorCode,
                                /*moduleInformation*/ {},
                                appContext()->version()
                            );
                        }
                    },
                    this);

                disconnectFromServer(DisconnectFlag::Force);
            });
    }

    connect(this, &ConnectActionsHandler::stateChanged, this,
        &ConnectActionsHandler::updatePreloaderVisibility);

    auto sessionTokenExpirationWatcher =
        new LocalSessionTokenExpirationWatcher(
            context()->instance<workbench::LocalNotificationsManager>(),
            this);

    connect(
        sessionTokenExpirationWatcher,
        &LocalSessionTokenExpirationWatcher::authenticationRequested,
        this,
        [this]()
        {
            auto existingCredentials = systemContext()->connectionCredentials();

            QString mainText = systemContext()->userWatcher()->user()->isTemporary()
                ? tr("Enter access link to continue your session")
                : tr("Enter password to continue your session");

            QString infoText = systemContext()->userWatcher()->user()->isTemporary()
                ? tr("Your session has expired. Please sign in again with your link to continue.")
                : tr("Your session has expired. Please sign in again to continue.");

            const bool passwordValidationMode = existingCredentials.authToken.isPassword();
            const auto refreshSessionResult = SessionRefreshDialog::refreshSession(
                mainWindowWidget(),
                tr("Re-authentication required"),
                mainText,
                infoText,
                tr("OK", "Dialog button text."),
                /*warningStyledAction*/ false,
                passwordValidationMode);

            if (!refreshSessionResult.token.empty())
            {
                auto credentials = connection()->credentials();
                credentials.authToken = refreshSessionResult.token;

                if (auto messageProcessor = qnClientMessageProcessor; NX_ASSERT(messageProcessor))
                {
                    messageProcessor->holdConnection(
                        QnClientMessageProcessor::HoldConnectionPolicy::reauth);
                }

                connection()->updateCredentials(
                    credentials,
                    refreshSessionResult.tokenExpirationTime);

                const auto localSystemId = connection()->moduleInformation().localSystemId;
                const auto savedCredentials =
                    CredentialsManager::credentials(localSystemId, credentials.username);
                const bool passwordIsAlreadySaved =
                    savedCredentials && !savedCredentials->authToken.empty();
                if (passwordIsAlreadySaved)
                    CredentialsManager::storeCredentials(localSystemId, credentials);
            }
        });

    // The initialResourcesReceived signal may never be emitted if the server has issues.
    // Avoid infinite UI "Loading..." state by forcibly dropping the connections after a timeout.
    const auto connectTimeout = ini().connectTimeoutMs;
    if (connectTimeout > 0)
        setupConnectTimeoutTimer(milliseconds(connectTimeout));

    auto userWatcher = systemContext()->userWatcher();
    connect(userWatcher, &core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            QnPeerRuntimeInfo localInfo = systemContext()->runtimeInfoManager()->localInfo();
            localInfo.data.userId = user ? user->getId() : QnUuid();
            systemContext()->runtimeInfoManager()->updateLocalItem(localInfo);
        });

    // The watcher should be created here so it can monitor all permission changes.
    context()->instance<ContextCurrentUserWatcher>();

    connect(action(ui::action::ConnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_connectAction_triggered);

    connect(action(ui::action::SetupFactoryServerAction), &QAction::triggered, this,
        [this]()
        {
            const auto actionParameters = menu()->currentParameters(sender());
            if (NX_ASSERT(actionParameters.hasArgument(Qn::LogonDataRole)))
            {
                const auto logonData =
                    actionParameters.argument<LogonData>(Qn::LogonDataRole);
                const auto welcomeScreen = mainWindow()->welcomeScreen();
                if (NX_ASSERT(welcomeScreen)
                    && NX_ASSERT(logonData.expectedServerId))
                {
                    welcomeScreen->setupFactorySystem(
                        logonData.address,
                        logonData.expectedServerId.value());
                }
            }
        });

    connect(action(ui::action::ConnectToCloudSystemAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_connectToCloudSystemAction_triggered);
    connect(
        action(ui::action::ConnectToCloudSystemWithUserInteractionAction),
        &QAction::triggered,
        this,
        &ConnectActionsHandler::onConnectToCloudSystemWithUserInteractionTriggered);
    connect(action(ui::action::ReconnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_reconnectAction_triggered);

    connect(action(ui::action::DisconnectMainMenuAction), &QAction::triggered, this,
        [this]()
        {
            if (auto session = RemoteSession::instance())
                session->autoTerminateIfNeeded();

            at_disconnectAction_triggered();
        });

    connect(action(ui::action::DisconnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_disconnectAction_triggered);

    connect(action(ui::action::SelectCurrentServerAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_selectCurrentServerAction_triggered);

    connect(action(ui::action::OpenLoginDialogAction), &QAction::triggered, this,
        &ConnectActionsHandler::showLoginDialog, Qt::QueuedConnection);
    connect(action(ui::action::BeforeExitAction), &QAction::triggered, this,
        [this]()
        {
            disconnectFromServer(DisconnectFlag::Force);
            action(ui::action::ResourcesModeAction)->setChecked(true);
        });

    connect(action(ui::action::LogoutFromCloud), &QAction::triggered,
        this, &ConnectActionsHandler::at_logoutFromCloud);

    connect(systemContext()->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.uuid != systemContext()->peerId())
                return;

            if (auto connection = systemContext()->messageBusConnection())
            {
                connection->getMiscManager(Qn::kSystemAccess)->saveRuntimeInfo(
                    info.data, [](int /*requestId*/, ec2::ErrorCode) {});
            }
        });

    const auto resourceModeAction = action(ui::action::ResourcesModeAction);
    connect(resourceModeAction, &QAction::toggled, this,
        [this](bool checked)
        {
            // Check if action state was changed during queued connection.
            if (action(ui::action::ResourcesModeAction)->isChecked() != checked)
                return;

            if (mainWindow()->welcomeScreen())
                mainWindow()->setWelcomeScreenVisible(!checked);
            if (workbench()->layouts().empty())
                menu()->trigger(ui::action::OpenNewTabAction);
        });

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    qnClientCoreModule->networkModule()->connectionFactory()->setUserInteractionDelegate(
        createConnectionUserInteractionDelegate(mainWindowWidget()));

    // The only instance of UserAuthDebugInfoWatcher is created to be owned by the context.
    context()->instance<UserAuthDebugInfoWatcher>();

    auto temporeryUserExpirationWatcher =
        new TemporaryUserExpirationWatcher(
            context()->instance<workbench::LocalNotificationsManager>(),
            this);
}

ConnectActionsHandler::~ConnectActionsHandler()
{
}

ConnectActionsHandler::LogicalState ConnectActionsHandler::logicalState() const
{
    return d->logicalState;
}

void ConnectActionsHandler::setupConnectTimeoutTimer(milliseconds timeout)
{
    auto connectTimeoutTimer = new QTimer(this);

    connectTimeoutTimer->setSingleShot(true);
    connectTimeoutTimer->setInterval(timeout);

    connect(this, &ConnectActionsHandler::stateChanged, this,
        [connectTimeoutTimer](LogicalState logicalValue)
        {
            switch (logicalValue)
            {
                case LogicalState::connected:
                case LogicalState::disconnected:
                    connectTimeoutTimer->stop();
                    return;
                case LogicalState::connecting:
                    if (!connectTimeoutTimer->isActive())
                        connectTimeoutTimer->start();
                    return;
                default:
                    return;
            }
        });

    connect(connectTimeoutTimer, &QTimer::timeout, this,
        [this]()
        {
            const auto errorCode = RemoteConnectionErrorCode::connectionTimeout;
            d->handleWSError(errorCode);

            disconnectFromServer(DisconnectFlag::Force);
            QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                context(),
                errorCode,
                /*moduleInformation*/ {},
                appContext()->version());
        });
}

void ConnectActionsHandler::switchCloudHostIfNeeded(const QString& remoteCloudHost)
{
    if (remoteCloudHost.isEmpty())
        return;

    if (std::string(ini().cloudHost) != "auto")
        return;

    const auto activeCloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    if (activeCloudHost == remoteCloudHost)
        return;

    NX_DEBUG(this, "Switching cloud to %1", remoteCloudHost);
    menu()->trigger(ui::action::LogoutFromCloud);
    nx::network::SocketGlobals::switchCloudHost(remoteCloudHost.toStdString());
}

void ConnectActionsHandler::hideReconnectDialog()
{
    if (d->reconnectDialog)
    {
        d->reconnectDialog->disconnect(this);
        d->reconnectDialog->deleteLater();
        d->reconnectDialog.clear();
    }
}

void ConnectActionsHandler::handleConnectionError(RemoteConnectionError error)
{
    if (!NX_ASSERT(qnRuntime->isDesktopMode()))
        return;

    auto welcomeScreen = mainWindow()->welcomeScreen();
    const bool isCloudConnection = isConnectionToCloud(
        d->currentConnectionProcess->context->logonData);
    const bool connectingTileExists = welcomeScreen
        && welcomeScreen->connectingTileExists();
    if (connectingTileExists)
        welcomeScreen->openConnectingTile(error.code);
    else
        updatePreloaderVisibility();

    if (!NX_ASSERT(d->currentConnectionProcess, "Error can be handled only while we are connecting"))
        return;

    if (error.code == RemoteConnectionErrorCode::sessionExpired)
    {
        CredentialsManager::forgetStoredPassword(
            d->currentConnectionProcess->context->moduleInformation.localSystemId,
            d->currentConnectionProcess->context->logonData.credentials.username);
    }

    switch (error.code)
    {
        case RemoteConnectionErrorCode::factoryServer:
        {
            const auto welcomeScreen = mainWindow()->welcomeScreen();
            if (NX_ASSERT(welcomeScreen, "Welcome screen must exist in the desktop mode"))
            {
                welcomeScreen->setupFactorySystem(
                    d->currentConnectionProcess->context->logonData.address,
                    d->currentConnectionProcess->context->moduleInformation.id);
            }
            return;
        }
        case RemoteConnectionErrorCode::binaryProtocolVersionDiffers:
        case RemoteConnectionErrorCode::cloudHostDiffers:
        {
            // TODO: Should enter 'updating' mode
            if (QnConnectionDiagnosticsHelper::downloadAndRunCompatibleVersion(
                mainWindowWidget(),
                d->currentConnectionProcess->context->moduleInformation,
                d->currentConnectionProcess->context->logonData,
                appContext()->version()))
            {
                ConnectionInfo compatibilityInfo{
                    .address = d->currentConnectionProcess->context->logonData.address,
                    .credentials = d->currentConnectionProcess->context->logonData.credentials,
                    .userType = d->currentConnectionProcess->context->logonData.userType
                };
                // TODO: #sivanov Why are we doing this?
                storeConnectionRecord(
                    compatibilityInfo,
                    /*moduleInformation*/ {}, //< TODO: #sivanov Make sure it is not needed.
                    /*options*/ {});
                menu()->trigger(ui::action::DelayedForcedExitAction);
            }
            break;
        }
        default:
        {
            if (!connectingTileExists || isCloudConnection)
            {
                QnConnectionDiagnosticsHelper::showConnectionErrorMessage(context(),
                    error,
                    d->currentConnectionProcess->context->moduleInformation,
                    appContext()->version(),
                    mainWindowWidget());
            }
            break;
        }
    }
}

void ConnectActionsHandler::establishConnection(RemoteConnectionPtr connection)
{
    if (!NX_ASSERT(connection))
        return;

    setState(LogicalState::connecting);
    const auto session = std::make_shared<desktop::RemoteSession>(connection, systemContext());

    session->setStickyReconnect(appContext()->localSettings()->stickReconnectToServer());
    LogonData logonData = connection->createLogonData();
    const auto serverModuleInformation = connection->moduleInformation();
    QnUuid systemId(::helpers::getTargetSystemId(serverModuleInformation));

    connect(session.get(), &RemoteSession::stateChanged, this,
        [this, logonData, systemId](RemoteSession::State state)
        {
            NX_DEBUG(this, "Remote session state changed: %1", state);

            if (state == RemoteSession::State::reconnecting)
            {
                if (d->reconnectDialog)
                    return;

                d->reconnectDialog = new QnReconnectInfoDialog(mainWindowWidget());
                connect(d->reconnectDialog, &QDialog::rejected, this,
                    [this]()
                    {
                        if (context()->user())
                            appContext()->clientStateHandler()->clientDisconnected();

                        disconnectFromServer(DisconnectFlag::Force);
                        if (!qnRuntime->isDesktopMode())
                            menu()->trigger(ui::action::DelayedForcedExitAction);
                    });
                d->reconnectDialog->show();
            }
            else if (state == RemoteSession::State::connected) //< Connection established.
            {
                hideReconnectDialog();
                setState(LogicalState::connected);

                NX_ASSERT(qnRuntime->isVideoWallMode() || context()->user());

                integrations::connectionEstablished(
                    context()->user(),
                    systemContext()->messageBusConnection());

                // Reload all dialogs and dependent data.
                // It's done delayed because some things - for example systemSettings() -
                // are updated in QueuedConnection to initial notification or resource addition.
                const auto workbenchStateUpdate =
                    [this, logonData, systemId]()
                    {
                        context()->instance<QnWorkbenchStateManager>()->forcedUpdate();
                        context()->menu()->trigger(ui::action::InitialResourcesReceivedEvent);
                        workbench()->addSystem(systemId, logonData);
                    };

                executeLater(workbenchStateUpdate, this);

                /* In several seconds after connect show warnings. */
                executeDelayedParented(
                    [this]() { showWarnMessagesOnce(); },
                    kMessagesDelayMs,
                    this);
            }
        });
    connect(session.get(), &RemoteSession::reconnectingToServer, this,
        [this](const QnMediaServerResourcePtr& server)
        {
            NX_DEBUG(this, "Reconnecting to server: %1", server ? server->getName() : "none");

            if (NX_ASSERT(d->reconnectDialog))
                d->reconnectDialog->setCurrentServer(server);
        });

    connect(session.get(),
        &RemoteSession::reconnectFailed,
        this,
        [this, serverModuleInformation](RemoteConnectionErrorCode errorCode)
        {
            NX_DEBUG(this, "Reconnect failed with error: %1", errorCode);

            executeDelayedParented(
                [this, errorCode, serverModuleInformation]()
                {
                    d->handleWSError(errorCode);

                    QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                        context(),
                        errorCode,
                        serverModuleInformation,
                        appContext()->version()
                    );
                },
                this);
            disconnectFromServer(DisconnectFlag::Force);
        });

    connect(session.get(),
        &RemoteSession::reconnectRequired,
        this,
        &ConnectActionsHandler::at_reconnectAction_triggered,
        Qt::QueuedConnection);

    qnClientCoreModule->networkModule()->setSession(session);
    appContext()->currentSystemContext()->setSession(session);
    const auto welcomeScreen = mainWindow()->welcomeScreen();
    if (welcomeScreen) // Welcome Screen exists in the desktop mode only.
        welcomeScreen->connectionToSystemEstablished(systemId);

    // TODO: #sivanov Implement separate address and credentials handling.
    appContext()->clientStateHandler()->clientConnected(
        appContext()->localSettings()->restoreUserSessionData(),
        session->sessionId(),
        logonData);
}

void ConnectActionsHandler::storeConnectionRecord(
    core::ConnectionInfo connectionInfo,
    const nx::vms::api::ModuleInformation& info,
    ConnectionOptions options)
{
    /**
     * Note! We don't save connection to new systems. But we have to update
     * weights for any connection using its local id
     *
     * Also, we always store connection if StorePassword flag is set because it means
     * it is not initial connection to factory system.
     */

    if (connectionInfo.isTemporary())
        return;

    const bool storePassword = options.testFlag(StorePassword);
    if (!storePassword && ::helpers::isNewSystem(info))
        return;

    const auto localId = ::helpers::getLocalSystemId(info);

    if (options.testFlag(UpdateSystemWeight))
        appContext()->coreSettings()->updateWeightData(localId);

    const nx::network::http::Credentials& credentials = connectionInfo.credentials;

    const bool cloudConnection = connectionInfo.isCloud();

    // Stores local credentials after successful connection.
    if (!cloudConnection && options.testFlag(StoreSession))
    {
        NX_DEBUG(this,
            "Store connection record of %1 to the system %2",
            credentials.username,
            localId);

        NX_ASSERT(!credentials.authToken.value.empty());
        NX_DEBUG(this, storePassword
            ? "Password is set"
            : "Password must be cleared");

        nx::network::http::Credentials storedCredentials = credentials;
        if (!storePassword)
            storedCredentials.authToken = {};

        CredentialsManager::storeCredentials(localId, storedCredentials);
    }

    if (cloudConnection)
    {
        nx::network::url::Builder builder;
        builder.setHost(info.cloudHost);
        appContext()->localSettings()->lastUsedConnection =
            {builder.toUrl(), QnUuid(info.cloudSystemId)};
    }
    else if (storePassword)
    {
        // AutoLogin may be enabled while we are connected to the system,
        // so we have to always save the last used connection info.
        NX_ASSERT(!credentials.authToken.value.empty());
        NX_DEBUG(this,
            "Saving last used connection of %1 to the system %2",
            credentials.username,
            connectionInfo.address);

        nx::network::url::Builder builder;
        builder.setEndpoint(connectionInfo.address);
        builder.setUserName(credentials.username);
        appContext()->localSettings()->lastUsedConnection =
            {builder.toUrl(), /*systemId*/ localId};
    }
    else
    {
        // Clear the stored value to be sure that we wouldn't connect to the wrong system.
        appContext()->localSettings()->lastUsedConnection = {};
    }

    if (options.testFlag(StorePreferredCloudServer) && cloudConnection)
        appContext()->coreSettings()->setPreferredCloudServer(info.cloudSystemId, info.id);

    if (cloudConnection)
    {
        qnCloudStatusWatcher->logSession(info.cloudSystemId);
    }
    else
    {
        NX_ASSERT(!connectionInfo.address.address.toString().empty(),
            "Wrong host is going to be saved to the recent connections list");

        nx::network::url::Builder builder;
        builder.setEndpoint(connectionInfo.address);

        // Stores connection if it is local.
        appContext()->coreSettings()->storeRecentConnection(localId, info.systemName, builder.toUrl());
    }
}

void ConnectActionsHandler::showWarnMessagesOnce()
{
    if (d->logicalState != LogicalState::connected)
        return;

    /* We are just reconnected automatically, e.g. after update. */
    if (d->warnMessagesDisplayed)
        return;

    d->warnMessagesDisplayed = true;

    /* Collect and send crash dumps if allowed */
    d->crashReporter->scanAndReportAsync();

    menu()->triggerIfPossible(ui::action::VersionMismatchMessageAction);

    context()->instance<QnWorkbenchLicenseNotifier>()->checkLicenses();

    // Ask user for analytics storage locations (e.g. in the case of migration).
    const auto& servers = context()->resourcePool()->servers();
    if (std::any_of(servers.begin(), servers.end(),
        [](const auto& server)
        {
            return server->metadataStorageId().isNull()
                && nx::analytics::serverHasActiveObjectEngines(server);
        }))
    {
        menu()->triggerIfPossible(ui::action::ConfirmAnalyticsStorageAction);
    }
}

void ConnectActionsHandler::setState(LogicalState logicalValue)
{
    if (d->logicalState == logicalValue)
        return;

    d->logicalState = logicalValue;
    emit stateChanged(d->logicalState, QPrivateSignal());
}

void ConnectActionsHandler::connectToServerInNonDesktopMode(const LogonData& logonData)
{
    static constexpr auto kCloseTimeout = 10s;

    NX_VERBOSE(this, "Directly connecting to the server %1", logonData.address);

    auto callback = d->makeSingleConnectionCallback(
        [this, logonData](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                if (qnRuntime->isVideoWallMode())
                {
                    if (*error == RemoteConnectionErrorCode::forbiddenRequest)
                    {
                        SceneBanner::show(
                            tr("Video Wall is removed on the server and will be closed."),
                            kCloseTimeout);
                        const QnUuid videoWallId(
                            QString::fromStdString(logonData.credentials.username));
                        VideoWallShortcutHelper::setVideoWallAutorunEnabled(videoWallId, false);
                    }
                    else
                    {
                        SceneBanner::show(
                            tr("Could not connect to server. Video Wall will be closed."),
                            kCloseTimeout);
                    }
                }
                else if (qnRuntime->isAcsMode())
                {
                    SceneBanner::show(
                        tr("Could not connect to server. Application will be closed."),
                        kCloseTimeout);
                }
                executeDelayedParented(
                    [this]() { menu()->trigger(ui::action::ExitAction); },
                    kCloseTimeout,
                    this);
            }
            else
            {
                auto connection = std::get<RemoteConnectionPtr>(result);
                establishConnection(connection);
            }
        });

    NX_VERBOSE(this, "Executing connect to the %1", logonData.address);
    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback);
}

void ConnectActionsHandler::updatePreloaderVisibility()
{
    const auto welcomeScreen = mainWindow()->welcomeScreen();
    if (!welcomeScreen)
        return;

    const auto resourceModeAction = action(ui::action::ResourcesModeAction);

    switch (d->logicalState)
    {
        case LogicalState::disconnected:
        {
            // Show welcome screen, hide preloader.
            resourceModeAction->setChecked(false);
            welcomeScreen->setConnectingToSystem(/*systemId*/ {});
            if (!mainWindow()->isSystemTabBarUpdating())
                welcomeScreen->setGlobalPreloaderVisible(false);
            break;
        }
        case LogicalState::connecting:
        {
            // Show welcome screen, show preloader.
            welcomeScreen->setGlobalPreloaderVisible(true);
            resourceModeAction->setChecked(false);
            break;
        }
        case LogicalState::connected:
        {
            // Hide welcome screen.
            welcomeScreen->setConnectingToSystem(/*systemId*/ {});
            welcomeScreen->setGlobalPreloaderVisible(false);
            resourceModeAction->setChecked(true); //< Hides welcome screen
            welcomeScreen->setGlobalPreloaderEnabled(true);
            break;
        }
        default:
        {
            break;
        }
    }
}

void ConnectActionsHandler::at_connectAction_triggered()
{
    NX_VERBOSE(this, "Connect to server triggered");

    const auto actionParameters = menu()->currentParameters(sender());

    if (!qnRuntime->isDesktopMode())
    {
        connectToServerInNonDesktopMode(actionParameters.argument<LogonData>(Qn::LogonDataRole));
        return;
    }

    // TODO: Disconnect only if second connect success?

    if (d->logicalState == LogicalState::connected)
    {
        NX_VERBOSE(this, "Disconnecting from the current server first");
        // Ask user if he wants to save changes.
        if (!disconnectFromServer(DisconnectFlag::NoFlags))
        {
            NX_VERBOSE(this, "User cancelled the disconnect");

            // Login dialog sets auto-terminate if needed. If user cancelled the disconnect,
            // we should restore status-quo to make sure session won't be terminated
            // unconditionally.
            if (auto session = RemoteSession::instance())
                session->setAutoTerminate(false);
            return;
        }
    }
    else
    {
        // Break 'Connecting' state and clear workbench.
        NX_VERBOSE(this, "Forcefully cleaning the state");
        disconnectFromServer(DisconnectFlag::Force);
    }

    if (actionParameters.hasArgument(Qn::LogonDataRole))
    {
        const auto logonData = actionParameters.argument<LogonData>(Qn::LogonDataRole);
        NX_DEBUG(this, "Connecting to the server %1", logonData.address);
        NX_VERBOSE(this,
            "Connection flags: storeSession %1, storePassword %2, secondary %3",
            logonData.storeSession,
            logonData.storePassword,
            logonData.secondaryInstance);

        if (!logonData.address.isNull())
        {
            ConnectionOptions options(UpdateSystemWeight);
            if (logonData.storeSession)
                options |= StoreSession;

            if ((logonData.storePassword || appContext()->localSettings()->autoLogin())
                && NX_ASSERT(appContext()->localSettings()->saveCredentialsAllowed()))
            {
                options |= StorePassword;
            }

            // Ignore warning messages for now if one client instance is opened already.
            if (logonData.secondaryInstance)
                d->warnMessagesDisplayed = true;

            NX_VERBOSE(this, "Connecting to the server %1 with testing before",
                logonData.address);
            connectToServer(logonData, options);
        }
    }
    else if (NX_ASSERT(actionParameters.hasArgument(Qn::RemoteConnectionRole)))
    {
        // Connect using Login dialog.

        auto connection = actionParameters.argument<RemoteConnectionPtr>(
            Qn::RemoteConnectionRole);

        const bool isTemporaryUser =
            connection->userType() == nx::vms::api::UserType::temporaryLocal;

        if (!isTemporaryUser)
        {
            nx::utils::Url lastUrlForLoginDialog = nx::network::url::Builder()
                .setScheme(nx::network::http::kSecureUrlSchemeName)
                .setEndpoint(connection->address())
                .setUserName(QString::fromStdString(connection->credentials().username))
                .toUrl();

            appContext()->localSettings()->lastLocalConnectionUrl =
                lastUrlForLoginDialog.toString();
        }

        const auto savedCredentials = CredentialsManager::credentials(
            connection->moduleInformation().localSystemId,
            connection->credentials().username);
        const bool passwordIsAlreadySaved = savedCredentials
            && !savedCredentials->authToken.empty();

        // In most cases we will connect succesfully by this url. So we can store it.
        const bool storePasswordForTile = appContext()->localSettings()->saveCredentialsAllowed()
            && (passwordIsAlreadySaved || appContext()->localSettings()->autoLogin())
            && !isTemporaryUser;

        ConnectionOptions options;
        options.setFlag(StoreSession, !isTemporaryUser);
        options.setFlag(StorePassword, storePasswordForTile);

        storeConnectionRecord(
            connection->connectionInfo(),
            connection->moduleInformation(),
            options);
        establishConnection(connection);
    }
}

void ConnectActionsHandler::at_connectToCloudSystemAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto connectData = parameters.argument<CloudSystemConnectData>(
        Qn::CloudSystemConnectDataRole);
    const auto connectionInfo = core::cloudLogonData(connectData.systemId);

    if (!connectionInfo)
        return;

    statisticsModule()->certificates()->resetScenario();
    std::shared_ptr<QnStatisticsScenarioGuard> connectScenario = connectData.connectScenario
        ? statisticsModule()->certificates()->beginScenario(*connectData.connectScenario)
        : nullptr;

    auto callback = d->makeSingleConnectionCallback(
        [this, connectionInfo, connectScenario](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (auto error = std::get_if<RemoteConnectionError>(&result))
            {
                handleConnectionError(*error);
                if (logicalState() != LogicalState::connected)
                    setState(LogicalState::disconnected);
            }
            else
            {
                if (logicalState() == LogicalState::connected
                    && !disconnectFromServer(DisconnectFlag::NoFlags))
                {
                    return;
                }

                auto connection = std::get<RemoteConnectionPtr>(result);
                establishConnection(connection);
                storeConnectionRecord(
                    connection->connectionInfo(),
                    connection->moduleInformation(),
                    ConnectionOptions(UpdateSystemWeight | StoreSession));
            }
        });

    NX_VERBOSE(this, "Executing connect to the %1", connectionInfo->address);
    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();

    d->currentConnectionProcess = remoteConnectionFactory->connect(*connectionInfo, callback);
}

void ConnectActionsHandler::onConnectToCloudSystemWithUserInteractionTriggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto systemId = parameters.argument(Qn::CloudSystemIdRole).toString();
    auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    if (!NX_ASSERT(context && context->status() == CloudCrossSystemContext::Status::connectionFailure))
        return;

    context->initializeConnectionWithUserInteraction();
}

void ConnectActionsHandler::at_reconnectAction_triggered()
{
    if (d->logicalState != LogicalState::connected)
        return;

    // Reconnect call must not be executed while we are disconnected.
    auto currentConnection = this->connection();
    if (!currentConnection)
        return;

    // Try to login with the same address and credentials. Main reasons for the reconnect action
    // are user permissions change and/os system merge, so connection either will be successful or
    // correct diagnostic will be displayed.
    LogonData logonData(currentConnection->createLogonData());

    disconnectFromServer(DisconnectFlag::Force);

    // Do not store connections in case of reconnection
    NX_VERBOSE(this, "Reconnecting to the server %1", logonData.address);

    auto callback = d->makeSingleConnectionCallback(
        [this](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (auto error = std::get_if<RemoteConnectionError>(&result))
            {
                handleConnectionError(*error);
                setState(LogicalState::disconnected);
            }
            else
            {
                auto connection = std::get<RemoteConnectionPtr>(result);
                establishConnection(connection);
            }
        });

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback);
}

void ConnectActionsHandler::at_disconnectAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    DisconnectFlags flags = DisconnectFlag::ClearAutoLogin | DisconnectFlag::ManualDisconnect;
    if (parameters.hasArgument(Qn::ForceRole) && parameters.argument(Qn::ForceRole).toBool())
        flags |= DisconnectFlag::Force;

    NX_DEBUG(this, "Disconnecting from the server");
    const bool wasLoggedIn = (bool) context()->user();
    if (!disconnectFromServer(flags))
        return;

    if (wasLoggedIn)
        appContext()->clientStateHandler()->clientDisconnected();

    menu()->trigger(ui::action::RemoveSystemFromTabBarAction);
    emit mainWindow()->welcomeScreen()->dropConnectingState();
}

void ConnectActionsHandler::at_selectCurrentServerAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto server = parameters.resource().dynamicCast<QnMediaServerResource>();

    if (!NX_ASSERT(server, "Checked in the action conditions"))
        return;

    const auto serverId = server->getId();

    auto currentConnection = this->connection();
    if (!NX_ASSERT(currentConnection))
        return;

    if (!NX_ASSERT(serverId != currentConnection->moduleInformation().id, "Checked in the action conditions"))
        return;

    const auto discoveryManager = appContext()->moduleDiscoveryManager();
    const auto endpoint = discoveryManager->getEndpoint(serverId);
    if (!NX_ASSERT(endpoint, "Checked in the action conditions"))
    {
        reportServerSelectionFailure();
        return;
    }

    const auto systemId = systemSettings()->cloudSystemId();
    const auto currentUser = context()->user();

    core::LogonData logonData{
        .address = *endpoint,
        .credentials = currentConnection->credentials(),
        .userType = currentConnection->userType(),
        .expectedServerId = serverId};

    if (isConnectionToCloud(logonData))
    {
        logonData.address.address = helpers::serverCloudHost(systemId, serverId);
    }

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();

    if (d->switchServerDialog)
        delete d->switchServerDialog.data();
    d->switchServerDialog = new ConnectingToServerDialog(mainWindowWidget());
    d->switchServerDialog->setDisplayedServer(server);

    std::shared_ptr<QnStatisticsScenarioGuard> connectScenario =
        statisticsModule()->certificates()->beginScenario(
            ConnectScenario::connectFromTree);

    auto callback = d->makeSingleConnectionCallback(
        nx::utils::guarded(d->switchServerDialog.data(),
            [this, dialog = d->switchServerDialog, connectScenario]
                (RemoteConnectionFactory::ConnectionOrError result)
        {
            if (NX_ASSERT(dialog))
                delete dialog.data();

            if (std::get_if<RemoteConnectionError>(&result))
            {
                reportServerSelectionFailure();
            }
            else
            {
                if (!disconnectFromServer(DisconnectFlag::SwitchingServer))
                    return;

                auto connection = std::get<RemoteConnectionPtr>(result);
                establishConnection(connection);

                ConnectionOptions options(StoreSession | StorePreferredCloudServer);
                const auto localId = ::helpers::getLocalSystemId(connection->moduleInformation());
                const auto storedCredentials = CredentialsManager::credentials(
                    localId,
                    connection->credentials().username);
                const bool hasStoredCredentials = storedCredentials
                    && !storedCredentials->authToken.value.empty();

                // If current connection is stored, also save credentials to the target server.
                if (hasStoredCredentials)
                    options.setFlag(StorePassword);

                storeConnectionRecord(
                    connection->connectionInfo(),
                    connection->moduleInformation(),
                    options);
            }
        }));

    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback);

    const auto showModalDialog =
        [dialog = d->switchServerDialog]()
        {
            if (dialog)
                dialog->open();
        };

    executeDelayedParented(showModalDialog, kSelectCurrentServerShowDialogDelay, this);
}

void ConnectActionsHandler::at_logoutFromCloud()
{
    bool forceDisconnect = false;

    switch (d->logicalState)
    {
        case LogicalState::disconnected:
        case LogicalState::connecting:
            if (d->currentConnectionProcess &&
                isConnectionToCloud(d->currentConnectionProcess->context->logonData))
            {
                forceDisconnect = true;
            }
            break;
        case LogicalState::connected:
            forceDisconnect = connection()->connectionInfo().isCloud();
            break;
        default:
            NX_ASSERT(false, "Unhandled connection state: %1", d->logicalState);
    }

    if (forceDisconnect)
        disconnectFromServer(DisconnectFlag::Force | DisconnectFlag::ClearAutoLogin);
}

bool ConnectActionsHandler::disconnectFromServer(DisconnectFlags flags)
{
    NX_DEBUG(this, "Disconnecting from the current server (flags %1)", int(flags));
    const bool force = flags.testFlag(DisconnectFlag::Force);

    if (flags.testFlag(DisconnectFlag::ClearAutoLogin))
        appContext()->localSettings()->lastUsedConnection = {};

    appContext()->clientStateHandler()->storeSystemSpecificState();
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (flags.testFlag(DisconnectFlag::SwitchingServer))
    {
        setState(LogicalState::connecting);
    }
    else if (d->logicalState != LogicalState::disconnected)
    {
        setState(LogicalState::disconnected);
        if (auto welcomeScreen = mainWindow()->welcomeScreen())
            welcomeScreen->dropConnectingState();
    }
    if (force)
        menu()->trigger(ui::action::RemoveSystemFromTabBarAction);

    clearConnection();
    return true;
}

void ConnectActionsHandler::showLoginDialog()
{
    if (context()->closingDown())
        return;

    auto dialog = std::make_unique<LoginDialog>(mainWindowWidget());
    dialog->exec();
}

void ConnectActionsHandler::clearConnection()
{
    NX_DEBUG(this, "Clear connection: starting");

    hideReconnectDialog();
    d->currentConnectionProcess.reset();
    statisticsModule()->certificates()->resetScenario();
    qnClientCoreModule->networkModule()->setSession({});
    appContext()->currentSystemContext()->setSession({});

    context()->instance<QnWorkbenchStateManager>()->tryClose(/*force*/ true);

    // Get ready for the next connection.
    d->warnMessagesDisplayed = false;

    // Remove all remote resources.
    auto resourcesToRemove = resourcePool()->getResourcesWithFlag(Qn::remote);

    // Also remove layouts that were just added and have no 'remote' flag set.
    for (const auto& layout: resourcePool()->getResources<QnLayoutResource>())
    {
        if (layout->hasFlags(Qn::local) && !layout->isFile())  //< Do not remove exported layouts.
            resourcesToRemove.push_back(layout);
    }

    for (const auto& aviResource: resourcePool()->getResources<QnAviResource>())
    {
        if (!aviResource->isOnline() && !aviResource->isEmbedded())
            resourcesToRemove.push_back(aviResource);
    }

    for (const auto& fileLayoutResource: resourcePool()->getResources<QnFileLayoutResource>())
    {
        if (!fileLayoutResource->isOnline())
            resourcesToRemove.push_back(fileLayoutResource);
    }

    resourceAccessManager()->beginUpdate();

    QVector<QnUuid> idList;
    idList.reserve(resourcesToRemove.size());
    for (const auto& res: resourcesToRemove)
        idList.push_back(res->getId());

    NX_DEBUG(this, "Clear connection: removing resources");
    resourcePool()->removeResources(resourcesToRemove);

    systemContext()->showreelManager()->resetShowreels();

    systemContext()->resourcePropertyDictionary()->clear(idList);
    systemContext()->resourceStatusDictionary()->clear(idList);
    systemContext()->licensePool()->reset();

    resourceAccessManager()->endUpdate();

    systemContext()->lookupListManager()->deinitialize();

    NX_DEBUG(this, "Clear connection: finished");
}

void ConnectActionsHandler::connectToServer(LogonData logonData, ConnectionOptions options)
{
    std::shared_ptr<QnStatisticsScenarioGuard> connectScenario = logonData.connectScenario
        ? statisticsModule()->certificates()->beginScenario(*logonData.connectScenario)
        : nullptr;

    // Store username case-sensitive as it was entered (actual only for the digest auth method).
    std::string originalUsername = logonData.credentials.username;
    const auto isLink = logonData.connectScenario == ConnectScenario::connectUsingCommand;

    auto callback = d->makeSingleConnectionCallback(
        [this, options, connectScenario, originalUsername, isLink]
            (RemoteConnectionFactory::ConnectionOrError result)
        {
            if (auto error = std::get_if<RemoteConnectionError>(&result))
            {
                if (error->code == RemoteConnectionErrorCode::unauthorized && isLink)
                    error->externalDescription = tr("Authentication details are incorrect");

                handleConnectionError(*error);
                disconnectFromServer(DisconnectFlag::Force);
            }
            else
            {
                auto connection = std::get<RemoteConnectionPtr>(result);
                switchCloudHostIfNeeded(
                    d->currentConnectionProcess->context->moduleInformation.cloudHost);

                establishConnection(connection);

                ConnectionInfo storedConnectionInfo = connection->connectionInfo();
                storedConnectionInfo.credentials.username = originalUsername;

                storeConnectionRecord(
                    storedConnectionInfo,
                    connection->moduleInformation(),
                    options);
            }
        });

    auto remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();
    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback);
}

void ConnectActionsHandler::reportServerSelectionFailure()
{
    QnMessageBox::critical(mainWindowWidget(), tr("Failed to connect to the selected server"));
}

} // namespace nx::vms::client::desktop
