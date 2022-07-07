// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_actions_handler.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtWidgets/QAction>

#include <api/runtime_info_manager.h>
#include <client_core/client_core_module.h>
#include <client_core/client_core_settings.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/desktop_client_message_processor.h>
#include <common/common_module.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <finders/systems_finder.h>
#include <helpers/system_helpers.h>
#include <helpers/system_weight_helper.h>
#include <licensing/license.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_misc_manager.h>
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
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/cloud_system_endpoint.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/utils/reconnect_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/session_manager/session_manager.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connection_delegate_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/connecting_to_server_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_license_notifier.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench.h>
#include <utils/applauncher_utils.h>
#include <utils/common/delayed.h>
#include <utils/connection_diagnostics_helper.h>
#include <watchers/cloud_status_watcher.h>

#include "../ui/login_dialog.h"
#include "../ui/welcome_screen.h"
#include "context_current_user_watcher.h"
#include "local_session_token_expiration_watcher.h"
#include "remote_session.h"
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
    std::shared_ptr<RemoteSession> session;
    QPointer<QnReconnectInfoDialog> reconnectDialog;
    QPointer<ConnectingToServerDialog> switchServerDialog;
    RemoteConnectionFactory::ProcessPtr currentConnectionProcess;
    /** Flag that we should handle new connection. */
    bool warnMessagesDisplayed = false;
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
    d->crashReporter = std::make_unique<ec2::CrashReporter>(commonModule());

    // Videowall must not disconnect automatically as we may have not option to restart it.
    // ACS clients display only fixed part of the archive, so they look quite safe.
    if (qnRuntime->isDesktopMode())
    {
        auto sessionTimeoutWatcher = qnClientCoreModule->networkModule()->sessionTimeoutWatcher();
        connect(sessionTimeoutWatcher, &RemoteSessionTimeoutWatcher::sessionExpired, this,
            [this]()
            {
                executeDelayedParented(
                    [this]()
                    {
                        const auto errorCode = RemoteConnectionErrorCode::sessionExpired;
                        d->handleWSError(errorCode);

                        QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
                            context(),
                            errorCode,
                            /*moduleInformation*/ {},
                            appContext()->version()
                        );
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
            const auto refreshSessionResult = SessionRefreshDialog::refreshSession(
                mainWindowWidget(),
                tr("Re-authentication required"),
                tr("Enter password to continue your session"),
                tr("Your session has expired. Please sign in again to continue."),
                tr("OK", "Dialog button text."),
                /*warningStyledAction*/ false,
                /*passwordValidationMode*/ connectionCredentials().authToken.isPassword());

            if (!refreshSessionResult.token.empty())
            {
                auto credentials = connection()->credentials();
                credentials.authToken = refreshSessionResult.token;
                connection()->updateCredentials(
                    credentials,
                    refreshSessionResult.tokenExpirationTime);

                const auto localSystemId = connection()->moduleInformation().localSystemId;
                nx::vms::client::core::CredentialsManager::storeCredentials(
                    localSystemId,
                    credentials);
            }
        });

    // The initialResourcesReceived signal may never be emitted if the server has issues.
    // Avoid infinite UI "Loading..." state by forcibly dropping the connections after a timeout.
    const auto connectTimeout = ini().connectTimeoutMs;
    if (connectTimeout > 0)
        setupConnectTimeoutTimer(milliseconds(connectTimeout));

    auto userWatcher = context()->instance<ContextCurrentUserWatcher>();
    connect(userWatcher, &ContextCurrentUserWatcher::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
            localInfo.data.userId = user ? user->getId() : QnUuid();
            runtimeInfoManager()->updateLocalItem(localInfo);
        });

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

    connect(action(ui::action::LogoutFromCloud), &QAction::triggered, this,
        [this]()
        {
            switch (d->logicalState)
            {
                case LogicalState::disconnected:
                case LogicalState::connecting:
                    if (d->currentConnectionProcess &&
                        isConnectionToCloud(d->currentConnectionProcess->context->logonData))
                    {
                        /**
                         * TODO: #ynikitenkov Get rid of this static cast (here and below).
                         * Write #define like Q_DECLARE_OPERATORS_FOR_FLAGS for private
                         * class flags
                         */
                        const auto flags = static_cast<DisconnectFlags>(DisconnectFlag::Force
                            /*| DisconnectFlag::ErrorReason*/ | DisconnectFlag::ClearAutoLogin);
                        disconnectFromServer(flags);
                    }
                    return;
                case LogicalState::connected:
                    break;
                default:
                    NX_ASSERT(false, "Unhandled connection state");
            }

            // There is no way to login with different cloud users since 5.0.
            // Disconnect unconditionally.
            disconnectFromServer(static_cast<DisconnectFlags>(
                DisconnectFlag::Force | DisconnectFlag::ClearAutoLogin));
        });

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.uuid != commonModule()->peerId())
                return;

            if (auto connection = messageBusConnection())
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
            if (workbench()->layouts().isEmpty())
                menu()->trigger(ui::action::OpenNewTabAction);
        });

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    connect(context(), &QnWorkbenchContext::mainWindowChanged, this,
        [this]()
        {
            qnClientCoreModule->networkModule()->connectionFactory()->setUserInteractionDelegate(
                createConnectionUserInteractionDelegate(mainWindowWidget()));
        });

    // The only instance of UserAuthDebugInfoWatcher is created to be owned by the context.
    context()->instance<UserAuthDebugInfoWatcher>();
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
            if (!connectingTileExists || error.externalDescription || isCloudConnection)
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
    const auto session = std::make_shared<desktop::RemoteSession>(connection);

    session->setStickyReconnect(qnSettings->stickReconnectToServer());
    LogonData logonData = connection->createLogonData();
    const auto serverModuleInformation = connection->moduleInformation();
    QnUuid systemId(::helpers::getTargetSystemId(serverModuleInformation));

    connect(session.get(), &RemoteSession::stateChanged, this,
        [this, logonData, systemId](RemoteSession::State state)
        {
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

                integrations::connectionEstablished(context()->user(), messageBusConnection());

                // Reload all dialogs and dependent data.
                // It's done delayed because some things - for example globalSettings() -
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
            if (NX_ASSERT(d->reconnectDialog))
                d->reconnectDialog->setCurrentServer(server);
        });

    connect(session.get(),
        &RemoteSession::reconnectFailed,
        this,
        [this, serverModuleInformation](RemoteConnectionErrorCode errorCode)
        {
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

    const QString userName = QString::fromStdString(connection->credentials().username);

    context()->setUserName(userName);

    // TODO: #sivanov Implement separate address and credentials handling.
    appContext()->clientStateHandler()->clientConnected(
        qnSettings->restoreUserSessionData(),
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

    const bool storePassword = options.testFlag(StorePassword);
    if (!storePassword && ::helpers::isNewSystem(info))
        return;

    const auto localId = ::helpers::getLocalSystemId(info);

    if (options.testFlag(UpdateSystemWeight))
        nx::vms::client::core::helpers::updateWeightData(localId);

    const nx::network::http::Credentials& credentials = connectionInfo.credentials;

    const bool cloudConnection = connectionInfo.isCloud();

    // Stores local credentials after successful connection.
    if (!cloudConnection && options.testFlag(StoreSession))
    {
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag,
            "Store connection record of %1 to the system %2",
            credentials.username,
            localId);

        NX_ASSERT(!credentials.authToken.value.empty());
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag, storePassword
            ? "Password is set"
            : "Password must be cleared");

        nx::network::http::Credentials storedCredentials = credentials;
        if (!storePassword)
            storedCredentials.authToken = {};

        CredentialsManager::storeCredentials(localId, storedCredentials);
        qnClientCoreSettings->save();
    }

    if (storePassword)
    {
        // AutoLogin may be enabled while we are connected to the system,
        // so we have to always save the last used connection info.
        NX_ASSERT(!credentials.authToken.value.empty());
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag,
            "Saving last used connection of %1 to the system %2",
            credentials.username,
            connectionInfo.address);

        nx::network::url::Builder builder;
        builder.setEndpoint(connectionInfo.address);
        builder.setUserName(credentials.username);
        qnSettings->setLastUsedConnection({builder.toUrl(), /*systemId*/ localId});
    }
    else if (cloudConnection)
    {
        nx::network::url::Builder builder;
        builder.setHost(info.cloudHost);
        qnSettings->setLastUsedConnection({builder.toUrl(), QnUuid(info.cloudSystemId)});
    }
    else
    {
        // Clear the stored value to be sure that we wouldn't connect to the wrong system.
        qnSettings->setLastUsedConnection({});
    }
    qnSettings->save();

    if (options.testFlag(StorePreferredCloudServer) && cloudConnection)
        nx::vms::client::core::helpers::savePreferredCloudServer(info.cloudSystemId, info.id);

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
        nx::vms::client::core::helpers::storeConnection(localId, info.systemName, builder.toUrl());
        qnClientCoreSettings->save();
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
    d->crashReporter->scanAndReportAsync(qnSettings->rawSettings());

    menu()->triggerIfPossible(ui::action::AllowStatisticsReportMessageAction);
    menu()->triggerIfPossible(ui::action::VersionMismatchMessageAction);

    context()->instance<QnWorkbenchLicenseNotifier>()->checkLicenses();

    // Ask user for analytics storage locations (e.g. in the case of migration).
    const auto& servers = context()->resourcePool()->servers();
    if (std::any_of(servers.begin(), servers.end(),
        [this](const auto& server)
        {
            return server->metadataStorageId().isNull()
                && nx::analytics::serverHasActiveObjectEngines(commonModule(), server->getId());
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
    emit stateChanged(d->logicalState, {});
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

            if ((logonData.storePassword || qnSettings->autoLogin())
                && NX_ASSERT(qnSettings->saveCredentialsAllowed()))
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

        nx::utils::Url lastUrlForLoginDialog = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(connection->address())
            .setUserName(QString::fromStdString(connection->credentials().username))
            .toUrl();

        qnSettings->setLastLocalConnectionUrl(lastUrlForLoginDialog.toString());
        qnSettings->save();

        const auto savedCredentials = CredentialsManager::credentials(
            connection->moduleInformation().localSystemId,
            connection->credentials().username);
        const bool passwordIsAlreadySaved = savedCredentials
            && !savedCredentials->authToken.empty();

        // In most cases we will connect succesfully by this url. So we can store it.
        const bool storePasswordForTile = qnSettings->saveCredentialsAllowed()
            && (passwordIsAlreadySaved || qnSettings->autoLogin());

        ConnectionOptions options(StoreSession);
        if (storePasswordForTile)
            options.setFlag(StorePassword);

        establishConnection(connection);
        storeConnectionRecord(
            connection->connectionInfo(),
            connection->moduleInformation(),
            options);
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
    if (disconnectFromServer(flags) && wasLoggedIn)
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

    const auto systemId = globalSettings()->cloudSystemId();
    const auto currentUser = context()->user();

    core::LogonData logonData{
        .address = *endpoint,
        .credentials = currentConnection->credentials(),
        .userType = currentConnection->userType(),
        .expectedServerId = serverId};

    if (isConnectionToCloud(logonData) && server->hasInternetAccess())
    {
        logonData.address.address = nx::vms::client::core::helpers::serverCloudHost(
            systemId, serverId);
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
            [this, serverId, dialog = d->switchServerDialog, connectScenario]
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
        [this, dialog = d->switchServerDialog]()
        {
            if (dialog)
                dialog->open();
        };

    executeDelayedParented(showModalDialog, kSelectCurrentServerShowDialogDelay, this);
}

bool ConnectActionsHandler::disconnectFromServer(DisconnectFlags flags)
{
    NX_DEBUG(this, "Disconnecting from the current server (flags %1)", int(flags));
    const bool force = flags.testFlag(DisconnectFlag::Force);

    if (flags.testFlag(DisconnectFlag::ClearAutoLogin))
    {
        qnSettings->setLastUsedConnection({});
        qnSettings->save();
    }

    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (!force)
        globalSettings()->synchronizeNow();

    if (flags.testFlag(DisconnectFlag::SwitchingServer))
    {
        setState(LogicalState::connecting);
    }
    else
    {
        setState(LogicalState::disconnected);
        if (auto welcomeScreen = mainWindow()->welcomeScreen())
            welcomeScreen->dropConnectingState();
    }

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
    hideReconnectDialog();
    d->currentConnectionProcess.reset();
    qnClientCoreModule->networkModule()->setSession({});
    appContext()->currentSystemContext()->setSession({});

    context()->setUserName(QString());
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
    resourceAccessProvider()->beginUpdate();

    QVector<QnUuid> idList;
    idList.reserve(resourcesToRemove.size());
    for (const auto& res: resourcesToRemove)
        idList.push_back(res->getId());

    NX_DEBUG(this, "Clear connection");
    resourcePool()->removeResources(resourcesToRemove);

    resourcePropertyDictionary()->clear(idList);
    statusDictionary()->clear(idList);

    licensePool()->reset();

    resourceAccessProvider()->endUpdate();
    resourceAccessManager()->endUpdate();
}

void ConnectActionsHandler::connectToServer(LogonData logonData, ConnectionOptions options)
{
    std::shared_ptr<QnStatisticsScenarioGuard> connectScenario = logonData.connectScenario
        ? statisticsModule()->certificates()->beginScenario(*logonData.connectScenario)
        : nullptr;

    // Store username case-sensitive as it was entered (actual only for the digest auth method).
    std::string originalUsername = logonData.credentials.username;

    auto callback = d->makeSingleConnectionCallback(
        [this, options, connectScenario, originalUsername]
            (RemoteConnectionFactory::ConnectionOrError result)
        {
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
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
