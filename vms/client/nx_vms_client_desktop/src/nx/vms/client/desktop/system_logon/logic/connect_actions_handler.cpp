// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_actions_handler.h"

#include <chrono>

#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QAction>

#include <api/http_client_pool.h>
#include <api/runtime_info_manager.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/desktop_client_message_processor.h>
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
#include <licensing/license.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx/analytics/utils.h>
#include <nx/build_info.h>
#include <nx/monitoring/hardware_information.h>
#include <nx/network/address_resolver.h>
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
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/utils/reconnect_helper.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/settings/user_specific_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/ui/dialogs/connecting_to_server_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/ec2/crash_reporter.h>
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
#include <ui/widgets/system_tab_bar_state_handler.h>
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

namespace nx::vms::client::desktop {

namespace {

static const int kMessagesDelayMs = 5000;

static constexpr auto kSelectCurrentServerShowDialogDelay = 250ms;

bool isConnectionToCloud(const LogonData& logonData)
{
    return logonData.userType == nx::vms::api::UserType::cloud;
}

bool userIsChannelPartner(const QnUserResourcePtr& user)
{
    return user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

bool systemContainsChannelPartnerUser(nx::vms::client::desktop::SystemContext* system)
{
    return system->resourcePool()->contains<UserResource>(
        [](const UserResourcePtr& user) { return userIsChannelPartner(user); });
}

void resetSession(SystemContext* system, std::shared_ptr<core::RemoteSession> session = {})
{
    // Prevent old session destruction during change.
    auto oldSession = system->session();
    NX_VERBOSE(NX_SCOPE_TAG, "Change session %1 -> %2", oldSession, session);
    if (oldSession)
        oldSession->close();

    appContext()->networkModule()->setSession(session);
    system->setSession(session);
}

} // namespace

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
    bool needReconnectToLastCloudSystem = false;
    // The identifier of the cloud system we last attempted to connect to. An attempt to connect
    // does not imply a successful connection, so we update the variable at the moment we send the
    // connection request to the system, not after the connection is established. This is necessary
    // for correct reconnection to the last system in case the connection attempt fails, such as
    // when a new refresh token needs to be issued.
    QString lastAttemptedConnectCloudSystemId;

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

    void reconnectToLastCloudSystemIfNeeded()
    {
        if (!needReconnectToLastCloudSystem)
            return;

        if (qnCloudStatusWatcher->status() != core::CloudStatusWatcher::Online)
            return;

        NX_DEBUG(this, "Try to reconnect to last cloud system: %1, login: %3",
            lastAttemptedConnectCloudSystemId, qnCloudStatusWatcher->cloudLogin());
        NX_ASSERT(this, !lastAttemptedConnectCloudSystemId.isEmpty());

        needReconnectToLastCloudSystem = false;

        q->connectToCloudSystem({
            lastAttemptedConnectCloudSystemId,
            /*connectScenario*/ std::nullopt,
            /*useCache*/ false});
    }

    void reconnectToCloudIfNeeded()
    {
        needReconnectToLastCloudSystem = true;

        auto cloudAuthDataHelper = std::make_unique<FreshSessionTokenHelper>(
            q->mainWindowWidget(),
            tr("Login to %1", "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::branding::cloudName()),
            FreshSessionTokenHelper::ActionType::issueRefreshToken);

        const auto newCloudAuthData = cloudAuthDataHelper->requestAuthData(
            [this] { return !needReconnectToLastCloudSystem; });

        needReconnectToLastCloudSystem = false;
        if (newCloudAuthData.empty())
            return;

        qnCloudStatusWatcher->setAuthData(
            newCloudAuthData,
            core::CloudStatusWatcher::AuthMode::update);

        if (auto session = q->system()->session(); session && session->connection())
        {
            session->updateCloudSessionToken();
        }
        else
        {
            needReconnectToLastCloudSystem = true;
            q->updatePreloaderVisibility();
            reconnectToLastCloudSystemIfNeeded();
        }
    }

    void showChannelPartnerUserNotification()
    {
        if (!q->system()->localSettings()->channelPartnerUserNotificationClosed())
            return;

        const bool isChannelPartnerUser = userIsChannelPartner(q->system()->user());
        const auto isPowerUser = q->system()->accessController()->hasPowerUserPermissions();
        if (isChannelPartnerUser || !isPowerUser)
            return;

        if (!systemContainsChannelPartnerUser(q->system()))
            return;

        const auto notificationsManager = q->workbench()->windowContext()->localNotificationsManager();
        const auto channelPartnerNotificationId = notificationsManager->add(
            tr("Channel Partner users have access to this site"),
            tr("Channel Partner users' access is managed at the Organization level, "
                "and they are not visible in site user management."
                "<br/><br/><a href=\"%1\">Learn more</a>")
            .arg(HelpTopic::relativeUrlForTopic(HelpTopic::Id::ChannelPartnerUser)),
            /*cancellable*/ true);
        notificationsManager->setIconPath(
            channelPartnerNotificationId, "20x20/Outline/warning.svg");
        notificationsManager->setLevel(
            channelPartnerNotificationId, QnNotificationLevel::Value::ImportantNotification);
        connect(notificationsManager, &workbench::LocalNotificationsManager::cancelRequested,
            [localSettings = q->system()->localSettings(),
            notificationsManager,
            channelPartnerNotificationId](
                const nx::Uuid& notificationId)
            {
                if (notificationId == channelPartnerNotificationId)
                    notificationsManager->remove(channelPartnerNotificationId);

                localSettings->channelPartnerUserNotificationClosed = true;
            });
    }
};

ConnectActionsHandler::ConnectActionsHandler(WindowContext* windowContext, QObject* parent):
    base_type(parent),
    WindowContextAware(windowContext),
    d(new Private(this))
{
    d->timerManager = std::make_unique<nx::utils::TimerManager>("CrashReportTimers");
    d->crashReporter = std::make_unique<ec2::CrashReporter>(
        system(),
        d->timerManager.get(),
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/crashReporter");

    // Videowall must not disconnect automatically as we may have not option to restart it.
    // ACS clients display only fixed part of the archive, so they look quite safe.
    if (qnRuntime->isDesktopMode())
    {
        auto sessionTimeoutWatcher = system()->sessionTimeoutWatcher();
        connect(sessionTimeoutWatcher, &RemoteSessionTimeoutWatcher::sessionExpired, this,
            [this](RemoteSessionTimeoutWatcher::SessionExpirationReason reason)
            {
                const auto connection = system()->connection();
                if (!NX_ASSERT(connection))
                    return;

                if (qnCloudStatusWatcher->status() == core::CloudStatusWatcher::UpdatingCredentials)
                    return;

                if (reason == RemoteSessionTimeoutWatcher::SessionExpirationReason::sessionExpired
                    && connection->connectionInfo().credentials.authToken.isPassword()
                    && connection->connectionInfo().userType == vms::api::UserType::local)
                {
                    // Digest credentials should be cleared right after session is expired because
                    // server does not watch it.
                    CredentialsManager::forgetStoredPassword(
                        ::helpers::getLocalSystemId(system()->moduleInformation()),
                        connection->credentials().username);
                }

                const bool isTemporaryUser = connection->connectionInfo().isTemporary();
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
                            auto errorCode = RemoteConnectionErrorCode::sessionExpired;

                            if (reason == SessionExpirationReason::temporaryTokenExpired)
                                errorCode = RemoteConnectionErrorCode::temporaryTokenExpired;
                            else if (reason == SessionExpirationReason::truncatedSessionToken)
                                errorCode = RemoteConnectionErrorCode::truncatedSessionToken;

                            d->handleWSError(errorCode);

                            if (errorCode == RemoteConnectionErrorCode::truncatedSessionToken)
                            {
                                d->reconnectToCloudIfNeeded();
                                return;
                            }

                            QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                                this->windowContext(),
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
            system(),
            windowContext->localNotificationsManager(),
            this);

    connect(
        sessionTokenExpirationWatcher,
        &LocalSessionTokenExpirationWatcher::authenticationRequested,
        this,
        [this]()
        {
            const auto isCloud = system()->connection()->connectionInfo().isCloud();
            NX_DEBUG(this, "Authentication requested, cloud: %1", isCloud);

            if (isCloud)
            {
                d->reconnectToCloudIfNeeded();
                return;
            }

            auto existingCredentials = system()->credentials();

            QString mainText = system()->userWatcher()->user()->isTemporary()
                ? tr("Enter access link to continue your session")
                : tr("Enter password to continue your session");

            QString infoText = system()->userWatcher()->user()->isTemporary()
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
                auto credentials = system()->connection()->credentials();
                credentials.authToken = refreshSessionResult.token;

                if (auto messageProcessor = system()->clientMessageProcessor();
                    NX_ASSERT(messageProcessor))
                {
                    messageProcessor->holdConnection(
                        QnClientMessageProcessor::HoldConnectionPolicy::reauth);
                }

                system()->connection()->updateCredentials(
                    credentials,
                    refreshSessionResult.tokenExpirationTime);

                const auto localSystemId = system()->localSystemId();
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

    auto userWatcher = system()->userWatcher();
    connect(userWatcher, &core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            auto userId = user ? user->getId() : nx::Uuid();
            system()->runtimeInfoManager()->updateLocalItem(
                [userId](auto* data)
                {
                    data->data.userId = userId;
                    return true;
                });
        });

    // The watcher should be created here so it can monitor all permission changes.
    workbenchContext()->instance<ContextCurrentUserWatcher>();

    connect(action(menu::ConnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_connectAction_triggered);

    connect(action(menu::SetupFactoryServerAction), &QAction::triggered, this,
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

    connect(action(menu::ConnectToCloudSystemAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_connectToCloudSystemAction_triggered);

    connect(
        action(menu::ConnectToCloudSystemWithUserInteractionAction),
        &QAction::triggered,
        this,
        &ConnectActionsHandler::onConnectToCloudSystemWithUserInteractionTriggered);

    connect(action(menu::ReconnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_reconnectAction_triggered);

    connect(action(menu::DisconnectMainMenuAction), &QAction::triggered, this,
        [this]()
        {
            const std::shared_ptr<RemoteSession> session = mainWindow()->system()->session();
            if (!NX_ASSERT(session,
                "Disconnect action should be not available when Client is not connected."))
            {
                return;
            }

            if (qnRuntime->isDesktopMode() && ini().enableMultiSystemTabBar)
            {
                if (askDisconnectConfirmation())
                    mainWindow()->titleBarStateStore()->removeSession(session->sessionId());
                return;
            }

            session->autoTerminateIfNeeded();
            at_disconnectAction_triggered();
        });

    connect(action(menu::DisconnectAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_disconnectAction_triggered);

    connect(action(menu::SelectCurrentServerAction), &QAction::triggered, this,
        &ConnectActionsHandler::at_selectCurrentServerAction_triggered);

    connect(action(menu::OpenLoginDialogAction), &QAction::triggered, this,
        &ConnectActionsHandler::showLoginDialog, Qt::QueuedConnection);

    connect(action(menu::BeforeExitAction), &QAction::triggered, this,
        [this]()
        {
            disconnectFromServer(DisconnectFlag::Force);
            action(menu::ResourcesModeAction)->setChecked(true);
        });

    connect(action(menu::LogoutFromCloud), &QAction::triggered,
        this, &ConnectActionsHandler::at_logoutFromCloud);

    connect(system()->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.uuid != system()->peerId())
                return;

            if (auto connection = system()->messageBusConnection())
            {
                connection->getMiscManager(nx::network::rest::kSystemSession)->saveRuntimeInfo(
                    info.data, [](int /*requestId*/, ec2::ErrorCode) {});
            }
        });

    const auto resourceModeAction = action(menu::ResourcesModeAction);
    connect(resourceModeAction, &QAction::toggled, this,
        [this](bool checked)
        {
            // Check if action state was changed during queued connection.
            if (action(menu::ResourcesModeAction)->isChecked() != checked)
                return;

            if (mainWindow()->welcomeScreen())
                mainWindow()->setWelcomeScreenVisible(!checked);

            // If we are connecting to the site, layout will be created in Workbench::update().
            const auto isLoggedIn = this->windowContext()->system()->user();
            if (!isLoggedIn && workbench()->layouts().empty())
                menu()->trigger(menu::OpenNewTabAction);
        });

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    // The only instance of UserAuthDebugInfoWatcher is created to be owned by the context.
    workbenchContext()->instance<UserAuthDebugInfoWatcher>();

    new TemporaryUserExpirationWatcher(this);

    connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::statusChanged, this,
        [this]
        {
            d->reconnectToLastCloudSystemIfNeeded();
        });

    connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::refreshTokenChanged, this,
        [this]
        {
            d->reconnectToLastCloudSystemIfNeeded();
        });
}

ConnectActionsHandler::~ConnectActionsHandler()
{
}

bool ConnectActionsHandler::askDisconnectConfirmation() const
{
    if (showOnceSettings()->disconnectFromSystem())
        return true;

    QnMessageBox messageBox(
        QnMessageBox::Icon::Question,
        tr("Are you sure you want to disconnect?"),
        "",
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        mainWindowWidget());
    messageBox.addButton(tr("Disconnect"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);
    messageBox.setCheckBoxEnabled();
    messageBox.setWindowTitle(tr("Disconnect"));
    if (messageBox.exec() == QDialogButtonBox::Cancel)
        return false;

    if (messageBox.isChecked())
        showOnceSettings()->disconnectFromSystem = true;
    return true;
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
                windowContext(),
                errorCode,
                /*moduleInformation*/ {},
                appContext()->version());
        });
}

void ConnectActionsHandler::switchCloudHostIfNeeded(const QString& remoteCloudHost)
{
    if (remoteCloudHost.isEmpty())
        return;

    if (!ini().isAutoCloudHostDeductionMode())
        return;

    const auto activeCloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    if (activeCloudHost == remoteCloudHost)
        return;

    NX_DEBUG(this, "Switching cloud to %1", remoteCloudHost);
    menu()->trigger(menu::LogoutFromCloud);
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
    NX_WARNING(this, "Connection failed: %1", error.toString());
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
                const auto stateStore = mainWindow()->titleBarStateStore();
                if (stateStore && !stateStore->state().sessions.empty())
                    emit welcomeScreen->connectionToSystemRevoked();
                else
                    menu()->trigger(menu::DelayedForcedExitAction);
            }
            break;
        }
        default:
        {
            if (!connectingTileExists || isCloudConnection)
            {
                if (error.code == RemoteConnectionErrorCode::truncatedSessionToken)
                {
                    d->reconnectToCloudIfNeeded();
                    return;
                }

                QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                    windowContext(),
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

    NX_INFO(this, "Establish connection to %1 as %2", connection->address(),
        connection->credentials().username);

    emit windowContext()->beforeSystemChanged();

    setState(LogicalState::connecting);
    const auto session = std::make_shared<desktop::RemoteSession>(connection, system());

    session->setStickyReconnect(appContext()->localSettings()->stickReconnectToServer());
    LogonData logonData = connection->createLogonData();
    const auto serverModuleInformation = connection->moduleInformation();
    auto systemId = ::helpers::getTargetSystemId(serverModuleInformation);

    connect(session.get(), &RemoteSession::stateChanged, this,
        [this, sessionId = session->sessionId(), serverModuleInformation, logonData, systemId](RemoteSession::State state)
        {
            NX_DEBUG(this, "Remote session state changed: %1", state);

            if (state == RemoteSession::State::reconnecting)
            {
                if (d->reconnectDialog)
                    return;

                d->reconnectDialog = new QnReconnectInfoDialog(mainWindowWidget());
                connect(d->reconnectDialog, &QDialog::rejected, this,
                    [this, sessionId]()
                    {
                        if (ini().enableMultiSystemTabBar)
                            mainWindow()->titleBarStateStore()->removeSession(sessionId);

                        disconnectFromServer(DisconnectFlag::Force);
                        if (!qnRuntime->isDesktopMode())
                            menu()->trigger(menu::DelayedForcedExitAction);
                    });
                d->reconnectDialog->show();
            }
            else if (state == RemoteSession::State::connected) //< Connection established.
            {
                hideReconnectDialog();
                setState(LogicalState::connected);

                NX_ASSERT(qnRuntime->isVideoWallMode() || system()->user());

                integrations::connectionEstablished(
                    system()->user(),
                    system()->messageBusConnection());

                d->showChannelPartnerUserNotification();

                // Reload all dialogs and dependent data.
                // It's done delayed because some things - for example systemSettings() -
                // are updated in QueuedConnection to initial notification or resource addition.
                const auto workbenchStateUpdate =
                    [this, sessionId, serverModuleInformation, logonData]()
                    {
                        menu()->trigger(menu::InitialResourcesReceivedEvent);

                        if (ini().enableMultiSystemTabBar)
                        {
                            mainWindow()->titleBarStateStore()->addSession(
                                MainWindowTitleBarState::SessionData(
                                    sessionId, serverModuleInformation, logonData));
                            mainWindow()->titleBarStateStore()->setActiveSessionId(sessionId);
                        }
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

    // Using queued connection to avoid session destruction during callback.
    connect(session.get(),
        &RemoteSession::reconnectFailed,
        this,
        [this, serverModuleInformation, cached=connection->isCached()](
            RemoteConnectionErrorCode errorCode)
        {
            NX_DEBUG(this, "Reconnect failed with error: %1", errorCode);

            disconnectFromServer(DisconnectFlag::Force);

            d->handleWSError(errorCode);

            if (cached && !serverModuleInformation.cloudSystemId.isEmpty())
            {
                NX_DEBUG(this, "Try reconnect to cloud system without cache %1",
                    d->lastAttemptedConnectCloudSystemId);
                connectToCloudSystem({
                    d->lastAttemptedConnectCloudSystemId,
                    /*connectScenario*/ std::nullopt,
                    /*useCache*/ false},
                    /*anyServer*/ true);
                return;
            }

            if (errorCode == RemoteConnectionErrorCode::truncatedSessionToken)
            {
                d->reconnectToCloudIfNeeded();
                return;
            }

            QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                windowContext(),
                errorCode,
                serverModuleInformation,
                appContext()->version()
            );
        },
        Qt::QueuedConnection);

    connect(session.get(),
        &RemoteSession::reconnectRequired,
        this,
        &ConnectActionsHandler::at_reconnectAction_triggered,
        Qt::QueuedConnection);

    if (ini().enableMultiSystemTabBar)
    {
        mainWindow()->titleBarStateStore()->addSession(
            MainWindowTitleBarState::SessionData(
                session->sessionId(), serverModuleInformation, logonData));
    }

    resetSession(system(), session);

    const auto welcomeScreen = mainWindow()->welcomeScreen();
    if (welcomeScreen) // Welcome Screen exists in the desktop mode only.
        welcomeScreen->connectionToSystemEstablished(systemId);

    // TODO: #sivanov Implement separate address and credentials handling.
    appContext()->clientStateHandler()->connectionToSystemEstablished(
        appContext()->localSettings()->restoreUserSessionData()
            && (!mainWindow()->titleBarStateStore()
                || !mainWindow()->titleBarStateStore()->state().hasWorkbenchState()),
        session->sessionId(),
        logonData);
}

void ConnectActionsHandler::storeConnectionRecord(
    const core::ConnectionInfo& connectionInfo,
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

    // Recent connection address info is stored anyway.
    core::appContext()->knownServerConnectionsWatcher()->saveConnection(info.id,
        connectionInfo.address);

    const bool storePassword = options.testFlag(StorePassword);
    if (!storePassword && ::helpers::isNewSystem(info))
        return;

    const auto localId = ::helpers::getLocalSystemId(info);

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
            {builder.toUrl(), nx::Uuid(info.cloudSystemId)};
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
        NX_ASSERT(!connectionInfo.address.address.isEmpty(),
            "Wrong host is going to be saved to the recent connections list");

        nx::network::url::Builder builder;
        builder.setEndpoint(connectionInfo.address);

        // Stores connection if it is local.
        appContext()->coreSettings()->storeRecentConnection(
            localId,
            info.systemName,
            info.version,
            builder.toUrl());
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

    workbenchContext()->instance<QnWorkbenchLicenseNotifier>()->checkLicenses();

    // Ask user for analytics storage locations (e.g. in the case of migration).
    const auto& servers = system()->resourcePool()->servers();
    if (std::any_of(servers.begin(), servers.end(),
        [](const auto& server)
        {
            return server->metadataStorageId().isNull()
                && nx::analytics::serverHasActiveObjectEngines(server);
        }))
    {
        menu()->triggerIfPossible(menu::ConfirmAnalyticsStorageAction);
    }
}

void ConnectActionsHandler::setState(LogicalState logicalValue)
{
    if (d->logicalState == logicalValue)
        return;

    NX_DEBUG(this, "Logical state change: %1 -> %2", d->logicalState, logicalValue);

    d->logicalState = logicalValue;
    emit stateChanged(d->logicalState, QPrivateSignal());
}

void ConnectActionsHandler::connectToServerInNonDesktopMode(const LogonData& logonData)
{
    static constexpr auto kCloseTimeout = 10s;

    auto callback = d->makeSingleConnectionCallback(
        [this, logonData](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                NX_WARNING(this, "Connection failed: %1", error->toString());
                if (qnRuntime->isVideoWallMode())
                {
                    if (*error == RemoteConnectionErrorCode::forbiddenRequest)
                    {
                        SceneBanner::show(
                            tr("Video Wall is removed on the server and will be closed."),
                            kCloseTimeout);
                        const nx::Uuid videoWallId(
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
                    [this]() { menu()->trigger(menu::ExitAction); },
                    kCloseTimeout,
                    this);
            }
            else
            {
                auto connection = std::get<RemoteConnectionPtr>(result);
                establishConnection(connection);
            }
        });

    NX_VERBOSE(this, "Connecting in non-desktop mode to the %1", logonData.address);
    auto remoteConnectionFactory = appContext()->networkModule()->connectionFactory();
    d->currentConnectionProcess =
        remoteConnectionFactory->connect(logonData, callback, system());
}

void ConnectActionsHandler::updatePreloaderVisibility()
{
    const auto welcomeScreen = mainWindow()->welcomeScreen();
    if (!welcomeScreen)
        return;

    NX_VERBOSE(this, "Update preloader visibility, state: %1, reconnect: %2",
        d->logicalState, d->needReconnectToLastCloudSystem);

    const auto resourceModeAction = action(menu::ResourcesModeAction);

    switch (d->logicalState)
    {
        case LogicalState::disconnected:
        {
            // Show welcome screen, hide preloader.
            welcomeScreen->setGlobalPreloaderVisible(d->needReconnectToLastCloudSystem);
            welcomeScreen->setConnectingToSystem(/*systemId*/ {});
            resourceModeAction->setChecked(false);
            break;
        }
        case LogicalState::connecting:
        {
            // Show welcome screen, show preloader.
            welcomeScreen->setGlobalPreloaderVisible(!welcomeScreen->connectingTileExists());
            resourceModeAction->setChecked(false);
            break;
        }
        case LogicalState::connected:
        {
            // Hide welcome screen.
            welcomeScreen->setConnectingToSystem(/*systemId*/ {});
            welcomeScreen->setGlobalPreloaderVisible(false);
            resourceModeAction->setChecked(true); //< Hides welcome screen
            break;
        }
        default:
        {
            NX_ASSERT(this, "Unexpected logical state: %1", d->logicalState);
            break;
        }
    }
}

void ConnectActionsHandler::connectToCloudSystem(
    const CloudSystemConnectData& connectData, bool anyServer)
{
    auto connectionInfo = core::cloudLogonData(connectData.systemId, connectData.useCache);
    d->lastAttemptedConnectCloudSystemId = connectData.systemId;

    if (!connectionInfo)
        return;

    // To make sure the cache is valid for the user.
    connectionInfo->credentials.username = qnCloudStatusWatcher->cloudLogin().toStdString();

    if (anyServer && nx::network::SocketGlobals::addressResolver().isCloudHostname(
        connectionInfo->address.address.toString()))
    {
        connectionInfo->address = network::SocketAddress(connectData.systemId.toStdString());
    }

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

    NX_VERBOSE(this, "Connecting to cloud site: %1", connectionInfo->address);
    auto remoteConnectionFactory = appContext()->networkModule()->connectionFactory();

    d->currentConnectionProcess =
        remoteConnectionFactory->connect(*connectionInfo, callback, system());
}

void ConnectActionsHandler::at_connectAction_triggered()
{
    NX_DEBUG(this, "Connect to server triggered, state: %1", d->logicalState);

    const auto actionParameters = menu()->currentParameters(sender());

    const auto logonData = actionParameters.hasArgument(Qn::LogonDataRole)
        ? std::make_optional(actionParameters.argument<LogonData>(Qn::LogonDataRole))
        : std::nullopt;

    if (!qnRuntime->isDesktopMode() && NX_ASSERT(logonData))
    {
        connectToServerInNonDesktopMode(*logonData);
        return;
    }

    // TODO: Disconnect only if second connect success?

    if (d->logicalState == LogicalState::connected)
    {
        NX_VERBOSE(this, "Disconnecting from the current server first");

        const bool fromSystemTabBar = logonData
            && logonData->connectScenario == ConnectScenario::connectFromTabBar;

        // Ask user if he wants to save changes.
        if (!disconnectFromServer(fromSystemTabBar
            ? DisconnectFlag::SwitchingSystemTabs
            : DisconnectFlag::NoFlags))
        {
            NX_VERBOSE(this, "User cancelled the disconnect");

            // Login dialog sets auto-terminate if needed. If user cancelled the disconnect,
            // we should restore status-quo to make sure session won't be terminated
            // unconditionally.
            if (auto session = system()->session())
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

    // Make sure we are disconnected now.
    NX_ASSERT(d->logicalState == LogicalState::disconnected, "Unexpected state: %1", d->logicalState);

    if (logonData)
    {
        NX_DEBUG(this, "Connecting to the server %1", logonData->address);
        NX_VERBOSE(this,
            "Connection flags: storeSession %1, storePassword %2, secondary %3",
            logonData->storeSession,
            logonData->storePassword,
            logonData->secondaryInstance);

        if (!logonData->address.isNull())
        {
            ConnectionOptions options(UpdateSystemWeight);
            if (logonData->storeSession)
                options |= StoreSession;

            if (logonData->storePassword
                && NX_ASSERT(appContext()->localSettings()->saveCredentialsAllowed()))
            {
                options |= StorePassword;
            }

            // Ignore warning messages for now if one client instance is opened already.
            if (logonData->secondaryInstance)
                d->warnMessagesDisplayed = true;

            NX_VERBOSE(this, "Connecting to the server %1 with testing before",
                logonData->address);

            connectToServer(*logonData, options);
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
            nx::Url lastUrlForLoginDialog = nx::network::url::Builder()
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

        // In most cases we will connect successfully by this url. So we can store it.
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

    connectToCloudSystem(connectData);
}

void ConnectActionsHandler::onConnectToCloudSystemWithUserInteractionTriggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto systemId = parameters.argument(Qn::CloudSystemIdRole).toString();
    auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
    if (!NX_ASSERT(context && context->status() == CloudCrossSystemContext::Status::connectionFailure))
        return;

    context->initializeConnection(/*allowUserInteraction*/ true);
}

void ConnectActionsHandler::at_reconnectAction_triggered()
{
    if (d->logicalState != LogicalState::connected)
        return;

    // Reconnect call must not be executed while we are disconnected.
    auto currentConnection = system()->connection();
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

    auto remoteConnectionFactory = appContext()->networkModule()->connectionFactory();
    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback, system());
}

void ConnectActionsHandler::at_disconnectAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    DisconnectFlags flags = DisconnectFlag::ClearAutoLogin | DisconnectFlag::ManualDisconnect;
    if (parameters.hasArgument(Qn::ForceRole) && parameters.argument(Qn::ForceRole).toBool())
        flags |= DisconnectFlag::Force;

    NX_DEBUG(this, "Disconnect action triggered");

    const std::shared_ptr<RemoteSession> session = system()->session();
    if (!session)
        return;

    const SessionId sessionId = session->sessionId();

    if (!disconnectFromServer(flags))
        return;

    if (const auto stateStore = mainWindow()->titleBarStateStore())
        stateStore->removeSession(sessionId);

    d->lastAttemptedConnectCloudSystemId = {};
    emit mainWindow()->welcomeScreen()->dropConnectingState();
}

void ConnectActionsHandler::at_selectCurrentServerAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto server = parameters.resource().dynamicCast<QnMediaServerResource>();

    if (!NX_ASSERT(server, "Checked in the action conditions"))
        return;

    const auto serverId = server->getId();

    auto currentConnection = system()->connection();
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

    const auto systemId = system()->globalSettings()->cloudSystemId();
    const auto currentUser = system()->user();

    core::LogonData logonData{
        .address = *endpoint,
        .credentials = currentConnection->credentials(),
        .userType = currentConnection->userType(),
        .expectedServerId = serverId};

    if (isConnectionToCloud(logonData))
    {
        logonData.address.address = helpers::serverCloudHost(systemId, serverId);
    }

    auto remoteConnectionFactory = appContext()->networkModule()->connectionFactory();

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

            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                NX_WARNING(this, "Connection failed: %1", error->toString());
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

    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback, system());

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
            forceDisconnect = system()->connection()->connectionInfo().isCloud();
            break;
        default:
            NX_ASSERT(false, "Unhandled connection state: %1", d->logicalState);
    }

    if (forceDisconnect)
        disconnectFromServer(DisconnectFlag::Force | DisconnectFlag::ClearAutoLogin);
}

bool ConnectActionsHandler::disconnectFromServer(DisconnectFlags flags)
{
    const bool force = flags.testFlag(DisconnectFlag::Force);
    const bool wasLoggedIn = !system()->user().isNull();
    NX_INFO(this, "Disconnect from server, state: %1, login: %2, flags: %3",
        d->logicalState, wasLoggedIn, (int) flags);

    if (flags.testFlag(DisconnectFlag::ClearAutoLogin))
        appContext()->localSettings()->lastUsedConnection = {};

    windowContext()->system()->userSettings()->saveImmediately();

    if (wasLoggedIn)
        appContext()->clientStateHandler()->storeSystemSpecificState();

    if (!windowContext()->workbenchStateManager()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (wasLoggedIn)
        appContext()->clientStateHandler()->clientDisconnected();

    if (flags.testFlag(DisconnectFlag::SwitchingServer)
        || flags.testFlag(DisconnectFlag::SwitchingSystemTabs))
    {
        setState(LogicalState::connecting);
    }
    else if (d->logicalState != LogicalState::disconnected)
    {
        setState(LogicalState::disconnected);
        if (auto welcomeScreen = mainWindow()->welcomeScreen())
            emit welcomeScreen->dropConnectingState();
    }

    emit windowContext()->beforeSystemChanged();

    hideReconnectDialog();

    NX_DEBUG(this, "Clearing local state");

    d->currentConnectionProcess.reset();
    statisticsModule()->certificates()->resetScenario();
    resetSession(system());

    // Get ready for the next connection.
    d->warnMessagesDisplayed = false;

    auto resourcePool = system()->resourcePool();
    // Remove all remote resources.
    auto resourcesToRemove = resourcePool->getResourcesWithFlag(Qn::remote);

    // Also remove layouts that were just added and have no 'remote' flag set.
    for (const auto& layout: resourcePool->getResources<QnLayoutResource>())
    {
        if (layout->hasFlags(Qn::local) && !layout->isFile())  //< Do not remove exported layouts.
            resourcesToRemove.push_back(layout);
    }

    for (const auto& aviResource: resourcePool->getResources<QnAviResource>())
    {
        if (!aviResource->isOnline() && !aviResource->isEmbedded())
            resourcesToRemove.push_back(aviResource);
    }

    for (const auto& fileLayoutResource: resourcePool->getResources<QnFileLayoutResource>())
    {
        if (!fileLayoutResource->isOnline())
            resourcesToRemove.push_back(fileLayoutResource);
    }

    system()->resourceAccessManager()->beginUpdate();

    QVector<nx::Uuid> idList;
    idList.reserve(resourcesToRemove.size());
    for (const auto& res: resourcesToRemove)
        idList.push_back(res->getId());

    NX_DEBUG(this, "Removing resources");
    resourcePool->removeResources(resourcesToRemove);

    system()->showreelManager()->resetShowreels();

    system()->resourcePropertyDictionary()->clear(idList);
    system()->resourceStatusDictionary()->clear(idList);
    system()->licensePool()->reset();

    system()->resourceAccessManager()->endUpdate();

    system()->lookupListManager()->deinitialize();
    system()->httpClientPool()->stop(/*invokeCallbacks*/ true);

    emit windowContext()->systemChanged();

    NX_DEBUG(this, "Disconnect finished");

    return true;
}

void ConnectActionsHandler::showLoginDialog()
{
    if (workbenchContext()->closingDown())
        return;

    auto dialog = std::make_unique<LoginDialog>(mainWindowWidget());
    dialog->exec();
}

void ConnectActionsHandler::connectToServer(LogonData logonData, ConnectionOptions options)
{
    std::shared_ptr<QnStatisticsScenarioGuard> connectScenario = logonData.connectScenario
        ? statisticsModule()->certificates()->beginScenario(*logonData.connectScenario)
        : nullptr;

    // Store username case-sensitive as it was entered (actual only for the digest auth method).
    std::string originalUsername = logonData.credentials.username;
    const auto isLink = logonData.connectScenario == ConnectScenario::connectUsingCommand;

    if (logonData.userType == nx::vms::api::UserType::cloud)
        d->lastAttemptedConnectCloudSystemId = logonData.expectedCloudSystemId.value_or(QString());

    auto callback = d->makeSingleConnectionCallback(
        [this, options, connectScenario, originalUsername, isLink, logonData]
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

    auto remoteConnectionFactory = appContext()->networkModule()->connectionFactory();
    d->currentConnectionProcess = remoteConnectionFactory->connect(logonData, callback, system());
}

void ConnectActionsHandler::reportServerSelectionFailure()
{
    QnMessageBox::critical(mainWindowWidget(), tr("Failed to connect to the selected server"));
}

} // namespace nx::vms::client::desktop
