#include "workbench_connect_handler.h"

#include <QtCore/QTimer>

#include <QtNetwork/QHostInfo>

#include <QtWidgets/QAction>

#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>

#include <client_core/client_core_settings.h>
#include <client/desktop_client_message_processor.h>

#include <finders/systems_finder.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>

#include <helpers/system_weight_helper.h>

#include <platform/hardware_information.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/integrations/integrations.h>

#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/common/message_box.h>

#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/items/resource/resource_widget.h>

#include <ui/widgets/main_window.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workbench/workbench_display.h>

#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_session_timeout_watcher.h>
#include <ui/workbench/workbench_license_notifier.h>

#include <utils/applauncher_utils.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/common/app_info.h>

#include <nx/utils/collection.h>
#include <utils/common/synctime.h>
#include <utils/common/delayed.h>
#include <nx/vms/discovery/manager.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <helpers/system_helpers.h>
#include <nx/client/core/utils/reconnect_helper.h>

#include <watchers/cloud_status_watcher.h>
#include <nx_ec/dummy_handler.h>
#include <nx/vms/client/desktop/system_logon/data/logon_parameters.h>
#include <nx/vms/client/desktop/videowall/utils.h>
#include <nx/vms/client/desktop/ui/dialogs/session_expired_dialog.h>

#include <nx/vms/client/desktop/ini.h>

#include <nx/analytics/utils.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

static const int kVideowallCloseTimeoutMSec = 10000;
static const int kMessagesDelayMs = 5000;
static constexpr int kReconnectDelayMs = 3000;

bool isConnectionToCloud(const nx::utils::Url& url)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host());
}

bool hasInternet(const QnMediaServerResourcePtr& server)
{
    return NX_ASSERT(server) && server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
}

nx::vms::api::ClientInfoData clientInfo()
{
    nx::vms::api::ClientInfoData clientData;
    clientData.id = qnSettings->pcUuid();
    clientData.fullVersion = nx::utils::AppInfo::applicationFullVersion();
    clientData.systemInfo = QnAppInfo::currentSystemInformation().toString();
    clientData.systemRuntime = nx::vms::api::SystemInformation::currentSystemRuntime();

    const auto& hw = HardwareInformation::instance();
    clientData.physicalMemory = hw.physicalMemory;
    clientData.cpuArchitecture = hw.cpuArchitecture;
    clientData.cpuModelName = hw.cpuModelName;

    const auto gl = QnGlFunctions::openGLInfo();
    clientData.openGLRenderer = gl.renderer;
    clientData.openGLVersion = gl.version;
    clientData.openGLVendor = gl.vendor;

    return clientData;
}

QString logicalToString(QnWorkbenchConnectHandler::LogicalState state)
{
    switch (state)
    {
        case QnWorkbenchConnectHandler::LogicalState::disconnected:
            return lit("disconnected");
        case QnWorkbenchConnectHandler::LogicalState::testing:
            return lit("testing");
        case QnWorkbenchConnectHandler::LogicalState::connecting:
            return lit("connecting");
        case QnWorkbenchConnectHandler::LogicalState::reconnecting:
            return lit("reconnecting");
        case QnWorkbenchConnectHandler::LogicalState::connecting_to_target:
            return lit("connecting_to_target");
        case QnWorkbenchConnectHandler::LogicalState::installing_updates:
            return lit("installing_updates");
        case QnWorkbenchConnectHandler::LogicalState::connected:
            return lit("connected");
        default:
            NX_ASSERT(false);
            break;
    }
    return QString();
}

QString physicalToString(QnWorkbenchConnectHandler::PhysicalState state)
{
    switch (state)
    {
        case QnWorkbenchConnectHandler::PhysicalState::disconnected:
            return lit("disconnected");
        case QnWorkbenchConnectHandler::PhysicalState::testing:
            return lit("testing");
        case QnWorkbenchConnectHandler::PhysicalState::waiting_peer:
            return lit("waiting_peer");
        case QnWorkbenchConnectHandler::PhysicalState::waiting_resources:
            return lit("waiting_resources");
        case QnWorkbenchConnectHandler::PhysicalState::connected:
            return lit("connected");
        default:
            NX_ASSERT(false);
            break;
    }
    return QString();
}

} //anonymous namespace

QDebug operator<<(QDebug dbg, QnWorkbenchConnectHandler::LogicalState state)
{
    dbg.nospace() << logicalToString(state);
    return dbg.space();
}

QDebug operator<<(QDebug dbg, QnWorkbenchConnectHandler::PhysicalState state)
{
    dbg.nospace() << physicalToString(state);
    return dbg.space();
}

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_crashReporter(commonModule())
{
    // This will work on its own.
    const auto sessionTimeoutWatcher = new WorkbenchSessionTimeoutWatcher(this);

    connect(this, &QnWorkbenchConnectHandler::stateChanged, this,
        &QnWorkbenchConnectHandler::handleStateChanged);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_initialResourcesReceived);

    // The initialResourcesReceived signal may never be emitted if the server has issues.
    // Avoid infinite UI "Loading..." state by forcibly dropping the connections after a timeout.
    const auto connectTimeout = ini().connectTimeoutMs;
    if (connectTimeout > 0)
        setupConnectTimeoutTimer(std::chrono::milliseconds(connectTimeout));

    auto userWatcher = context()->instance<QnWorkbenchUserWatcher>();
    connect(userWatcher, &QnWorkbenchUserWatcher::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
            localInfo.data.userId = user ? user->getId() : QnUuid();
            runtimeInfoManager()->updateLocalItem(localInfo);
        });

    connect(action(action::ConnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(action::ConnectToCloudSystemAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_connectToCloudSystemAction_triggered);
    connect(action(action::ReconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(action::DisconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_disconnectAction_triggered);
    connect(action(action::SelectCurrentServerAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_selectCurrentServerAction_triggered);

    connect(action(action::OpenLoginDialogAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::showLoginDialog, Qt::QueuedConnection);
    connect(action(action::BeforeExitAction), &QAction::triggered, this,
        [this]
        {
            disconnectFromServer(DisconnectFlag::Force);
            action(action::ResourcesModeAction)->setChecked(true);
        });

    connect(action(action::LogoutFromCloud), &QAction::triggered, this,
        [this]
        {
            switch (m_logicalState)
            {
                case LogicalState::disconnected:
                case LogicalState::testing:
                case LogicalState::installing_updates:
                    return;
                case LogicalState::connecting:
                case LogicalState::connecting_to_target:
                    if (isConnectionToCloud(m_connecting.url))
                    {
                        /**
                         * TODO: #ynikitenkov Get rid of this static cast (here and below).
                         * Write #define like Q_DECLARE_OPERATORS_FOR_FLAGS for private
                         * class flags
                         */
                        const auto flags = static_cast<DisconnectFlags>(DisconnectFlag::Force
                            | DisconnectFlag::ErrorReason | DisconnectFlag::ClearAutoLogin);
                        disconnectFromServer(flags);
                    }
                    return;
                case LogicalState::connected:
                case LogicalState::reconnecting:
                    break;
                default:
                    NX_ASSERT(false, "Unhandled connection state");
            }

            /* Check if we need to log out if logged in under this user. */
            QString currentLogin = commonModule()->currentUrl().userName();
            NX_ASSERT(!currentLogin.isEmpty());
            if (currentLogin.isEmpty())
                return;

            if (qnCloudStatusWatcher->effectiveUserName() == currentLogin)
            {
                disconnectFromServer(static_cast<DisconnectFlags>(
                    DisconnectFlag::Force | DisconnectFlag::ClearAutoLogin));
            }
        });

    context()->instance<ServerNotificationCache>();

    const auto resourceModeAction = action(action::ResourcesModeAction);
    connect(resourceModeAction, &QAction::toggled, this,
        [this](bool checked)
        {
            // Check if action state was changed during queued connection.
            if (action(action::ResourcesModeAction)->isChecked() != checked)
                return;

            if (mainWindow()->welcomeScreen())
                mainWindow()->setWelcomeScreenVisible(!checked);
            if (workbench()->layouts().isEmpty())
                action(action::OpenNewTabAction)->trigger();
        }, Qt::QueuedConnection); //< QueuedConnection is needed here because 2 title bars
                                  // (windowed/welcomescreen and fullscreen) are subscribed
                                  // to MainMenuAction, and main menu must not change
                                  // windowed/welcomescreen/fullscreen state
                                  // BETWEEN their slots processing MainMenuAction.
                                  //
                                  // TODO: #vkutin #gdm #ynikitenkov Lift this limitation in the future

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    //m_clientUpdateTool.reset(new nx::vms::client::desktop::ClientUpdateTool(this));
}

void QnWorkbenchConnectHandler::setupConnectTimeoutTimer(std::chrono::milliseconds timeout)
{
    auto connectTimeoutTimer = new QTimer(this);

    connectTimeoutTimer->setSingleShot(true);
    connectTimeoutTimer->setInterval(timeout);

    connect(this, &QnWorkbenchConnectHandler::stateChanged, this,
        [connectTimeoutTimer](LogicalState logicalValue, PhysicalState /*physicalValue*/)
        {
            switch (logicalValue)
            {
                case LogicalState::connected:
                case LogicalState::disconnected:
                    connectTimeoutTimer->stop();
                    return;
                case LogicalState::connecting:
                case LogicalState::connecting_to_target:
                    if (!connectTimeoutTimer->isActive())
                        connectTimeoutTimer->start();
                    return;
                default:
                    return;
            }
        });

    connect(connectTimeoutTimer, &QTimer::timeout, this,
        [this]
        {
            disconnectFromServer(DisconnectFlag::Force);
            // Just display the error message.
            QnConnectionDiagnosticsHelper::validateConnection(
                {},
                ec2::ErrorCode::serverError,
                mainWindowWidget(),
                commonModule()->engineVersion());
        });
}

QnWorkbenchConnectHandler::~QnWorkbenchConnectHandler()
{
}

QnWorkbenchConnectHandler::LogicalState QnWorkbenchConnectHandler::logicalState() const
{
    return m_logicalState;
}

QnWorkbenchConnectHandler::PhysicalState QnWorkbenchConnectHandler::physicalState() const
{
    return m_physicalState;
}

void QnWorkbenchConnectHandler::handleConnectReply(
    int handle,
    ec2::ErrorCode errorCode,
    ec2::AbstractECConnectionPtr connection)
{
    /* Check if we have entered 'connect' method again while were in 'connecting...' state */
    if (m_connecting.handle != handle)
    {
        NX_DEBUG(this, "handleConnectReply: waiting for another request, ignore");
        return;
    }

    if (m_logicalState == LogicalState::disconnected)
    {
        NX_DEBUG(this, "handleConnectReply: already disconnected, ignore");
        return;
    }

    if (m_physicalState != PhysicalState::testing)
    {
        NX_DEBUG(this, lm("handleConnectReply: invalid physical state %1").arg(physicalToString(m_physicalState)));
        return;
    }

    auto connectingStateGuard =
        nx::utils::makeScopeGuard([this]() { m_connecting.reset(); });

    /* Preliminary exit if application was closed while we were in the inner loop. */
    NX_ASSERT(!context()->closingDown());
    if (context()->closingDown())
    {
        NX_DEBUG(this, "handleConnectReply: closing application");
        return;
    }

    NX_ASSERT(connection || errorCode != ec2::ErrorCode::ok);
    QnConnectionInfo connectionInfo;
    if (connection)
        connectionInfo = connection->connectionInfo();

    const bool serverSwitch = m_connecting.storeConnection
        && m_logicalState == LogicalState::connecting_to_target;

    const bool silent = m_logicalState == LogicalState::reconnecting
        || serverSwitch
        || !qnRuntime->isDesktopMode();

    auto status = silent
        ? QnConnectionValidator::validateConnection(connectionInfo, errorCode)
        : QnConnectionDiagnosticsHelper::validateConnection(
            connectionInfo, errorCode, mainWindowWidget(),
            commonModule()->engineVersion());

    if (serverSwitch && status != Qn::ConnectionResult::SuccessConnectionResult)
        QnMessageBox::critical(mainWindowWidget(), tr("Failed to connect to the selected server"));

    NX_ASSERT(connection || status != Qn::SuccessConnectionResult);
    NX_DEBUG(this, lm("handleConnectReply: connection status %1").arg(status));

    if (m_logicalState == LogicalState::reconnecting)
    {
        connectingStateGuard.fire();
        processReconnectingReply(status, connection);
        return;
    }

    switch (status)
    {
        case Qn::SuccessConnectionResult:
            if (helpers::isNewSystem(connectionInfo) && !connectionInfo.ecDbReadOnly)
            {
                disconnectFromServer(DisconnectFlag::Force);
                if (const auto welcomeScreen = mainWindow()->welcomeScreen())
                {
                    /* Method is called from QML where we are passing QString. */
                    welcomeScreen->setupFactorySystem(connectionInfo.effectiveUrl().toString());
                }
            }
            else
            {
                establishConnection(connection);

                if (m_connecting.storeConnection)
                {
                    ConnectionOptions options(StoreSession | StorePreferredCloudServer);
                    const auto localId = helpers::getLocalSystemId(connectionInfo);
                    if (nx::vms::client::core::helpers::hasCredentials(localId))
                        options.setFlag(StorePassword);
                    if (qnSettings->autoLogin())
                        options.setFlag(AutoLogin);

                    storeConnectionRecord(m_connecting.url, connectionInfo, options);
                }
            }
            break;
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
            menu()->trigger(action::DelayedForcedExitAction);
            break;
        default:    //error
            if (qnRuntime->isVideoWallMode())
            {
                if (status == Qn::ForbiddenConnectionResult)
                {
                    QnGraphicsMessageBox::information(
                        tr("Video Wall is removed on the server and will be closed."),
                        kVideowallCloseTimeoutMSec);
                    const QnUuid videoWallId(m_connecting.url.userName());
                    setVideoWallAutorunEnabled(videoWallId, false);
                }
                else
                {
                    QnGraphicsMessageBox::information(
                        tr("Could not connect to server. Video Wall will be closed."),
                        kVideowallCloseTimeoutMSec);
                }
                executeDelayedParented(
                    [this]
                    {
                        action(action::ExitAction)->trigger();
                    }, kVideowallCloseTimeoutMSec, this
                );
            }
            else
            {
                disconnectFromServer(DisconnectFlag::Force | DisconnectFlag::ErrorReason);
            }
            break;
    }
}

void QnWorkbenchConnectHandler::processReconnectingReply(
    Qn::ConnectionResult status,
    ec2::AbstractECConnectionPtr connection)
{
    NX_ASSERT(m_reconnectHelper);
    if (!m_reconnectHelper)
    {
        disconnectFromServer(DisconnectFlag::Force);
        return;
    }

    NX_ASSERT(m_reconnectDialog && m_reconnectDialog->isVisible());
    bool success = status == Qn::SuccessConnectionResult;
    if (success)
    {
        NX_ASSERT(connection);
        /* Check if server was cleaned up during reconnect. */
        if (connection && connection->connectionInfo().newSystem)
        {
            m_reconnectHelper->markServerAsInvalid(m_reconnectHelper->currentServer());
            success = false;
        }
    }

    if (success)
    {
        stopReconnecting();
        establishConnection(connection);
        return;
    }

    switch (status)
    {
        // Server database was cleaned up during restart, e.g. merge to other system.
        case Qn::UnauthorizedConnectionResult:

        // Server was updated.
        case Qn::IncompatibleInternalConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
        case Qn::IncompatibleVersionConnectionResult:
        case Qn::IncompatibleProtocolConnectionResult:
            m_reconnectHelper->markServerAsInvalid(m_reconnectHelper->currentServer());
            break;
        default:
            break;
    }

    reconnectStep();
}

void QnWorkbenchConnectHandler::establishConnection(ec2::AbstractECConnectionPtr connection)
{
    NX_ASSERT(connection);
    if (!connection)
    {
        disconnectFromServer(DisconnectFlag::Force);
        return;
    }

    auto connectionInfo = connection->connectionInfo();

    setPhysicalState(PhysicalState::waiting_peer);
    nx::utils::Url url = connectionInfo.effectiveUrl();
    if (connectionInfo.ecUrl != url)
        connection->updateConnectionUrl(url);

    QnAppServerConnectionFactory::setEc2Connection(connection);
    qnClientMessageProcessor->init(connection);

    commonModule()->sessionManager()->start();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
        ? url.userName()
        : connectionInfo.effectiveUserName);

    integrations::connectionEstablished(connection);
}

void QnWorkbenchConnectHandler::storeConnectionRecord(
    nx::utils::Url url,
    const QnConnectionInfo& info,
    ConnectionOptions options)
{
    /**
     * Note! We don't save connection to new systems. But we have to update
     * weights for any connection using its local id
     *
     * Also, we always store connection if StorePassword flag is set because it means
     * it is not initial connection to factory system.
     */

    const bool autoLogin = options.testFlag(AutoLogin);
    const bool storePassword = options.testFlag(StorePassword) || autoLogin;
    if (!storePassword && helpers::isNewSystem(info))
        return;

    const auto localId = helpers::getLocalSystemId(info);

    if (options.testFlag(UpdateSystemWeight))
        nx::vms::client::core::helpers::updateWeightData(localId);

    const bool cloudConnection = isConnectionToCloud(url);

    // Stores local credentials for successful connection
    if (helpers::isLocalUser(url.userName()) && !cloudConnection && options.testFlag(StoreSession))
    {
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag,
            lm("Store connection record of %1 to the system %2").args(url.userName(), localId));

        NX_ASSERT(!url.password().isEmpty());
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag, storePassword
            ? "Password is set"
            : "Password must be cleared");

        const auto credentials = (storePassword
            ? nx::vms::common::Credentials(url)
            : nx::vms::common::Credentials(url.userName(), QString()));

        nx::vms::client::core::helpers::storeCredentials(localId, credentials);
        qnClientCoreSettings->save();
    }

    if (autoLogin)
    {
        NX_ASSERT(!url.password().isEmpty());
        NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag,
            lm("Saving last used connection of %1 to the system %2").args(url.userName(), url.host()));
        url.setPassword(QString());

        const auto lastUsed = QnConnectionData(info.systemName, url, localId);
        qnSettings->setLastUsedConnection(lastUsed);
    }

    qnSettings->setAutoLogin(autoLogin);
    qnSettings->save();

    if (options.testFlag(StorePreferredCloudServer) && cloudConnection)
        nx::vms::client::core::helpers::savePreferredCloudServer(info.cloudSystemId, info.serverId());

    if (cloudConnection)
    {
        qnCloudStatusWatcher->logSession(info.cloudSystemId);
    }
    else
    {
        NX_ASSERT(!url.host().isEmpty(),
            "Wrong host is going to be saved to the recent connections list");

        // Stores connection if it is local.
        nx::vms::client::core::helpers::storeConnection(localId, info.systemName, url);
        qnClientCoreSettings->save();
    }
}

void QnWorkbenchConnectHandler::showWarnMessagesOnce()
{
    if (m_logicalState != LogicalState::connected)
        return;

    /* We are just reconnected automatically, e.g. after update. */
    if (m_warnMessagesDisplayed)
        return;

    m_warnMessagesDisplayed = true;

    /* Collect and send crash dumps if allowed */
    m_crashReporter.scanAndReportAsync(qnSettings->rawSettings());

    menu()->triggerIfPossible(action::AllowStatisticsReportMessageAction);
    menu()->triggerIfPossible(action::VersionMismatchMessageAction);

    context()->instance<QnWorkbenchLicenseNotifier>()->checkLicenses();

    // Ask user for analytics storage locations (e.g. in the case of migration).
    const auto& servers = context()->resourcePool()->getAllServers(Qn::AnyStatus);
    if (std::any_of(servers.begin(), servers.end(),
        [this](const auto& server)
        {
            return server->metadataStorageId().isNull()
                && nx::analytics::hasActiveObjectEngines(commonModule(), server->getId());
        }))
    {
        menu()->triggerIfPossible(action::ConfirmAnalyticsStorageAction);
    }
}

void QnWorkbenchConnectHandler::stopReconnecting()
{
    if (m_reconnectDialog)
        m_reconnectDialog->deleteLater(); /*< We may get here from this dialog 'reject' handler. */
    m_reconnectDialog.clear();
    m_reconnectHelper.reset();
    m_connecting.reset();
}

void QnWorkbenchConnectHandler::setState(LogicalState logicalValue, PhysicalState physicalValue)
{
    if (m_logicalState == logicalValue && m_physicalState == physicalValue)
        return;
    m_logicalState = logicalValue;
    m_physicalState = physicalValue;
    emit stateChanged(m_logicalState, m_physicalState);
}

void QnWorkbenchConnectHandler::setLogicalState(LogicalState value)
{
    setState(value, m_physicalState);
}

void QnWorkbenchConnectHandler::setPhysicalState(PhysicalState value)
{
    setState(m_logicalState, value);
}

void QnWorkbenchConnectHandler::showPreloader()
{
    if (!qnRuntime->isDesktopMode())
        return;

    const auto welcomeScreen = mainWindow()->welcomeScreen();
    const auto resourceModeAction = action(action::ResourcesModeAction);

    resourceModeAction->setChecked(false); //< Shows welcome screen
    welcomeScreen->handleConnectingToSystem();
    welcomeScreen->setGlobalPreloaderVisible(true);
}

void QnWorkbenchConnectHandler::handleStateChanged(LogicalState logicalValue,
    PhysicalState physicalValue)
{
    const auto resourceModeAction = action(action::ResourcesModeAction);

    NX_DEBUG(this, "State changed logical=%1, physical=%2", logicalValue, physicalValue);
    switch (logicalValue)
    {
        case LogicalState::disconnected:
        {
            if (const auto welcomeScreen = mainWindow()->welcomeScreen())
            {
                welcomeScreen->handleDisconnectedFromSystem();
                welcomeScreen->setGlobalPreloaderVisible(false);
            }
            resourceModeAction->setChecked(false);  //< Shows welcome screen
            break;
        }
        case LogicalState::connecting_to_target:
            showPreloader();
            break;
        case LogicalState::connecting:
            if (physicalValue == PhysicalState::waiting_resources)
                showPreloader();
            break;
        case LogicalState::installing_updates:
            // TODO: Check current update status
            break;
        case LogicalState::connected:
            stopReconnecting();
            resourceModeAction->setChecked(true); //< Hides welcome screen
            break;
        default:
            break;
    }
}

void QnWorkbenchConnectHandler::reconnectStep()
{
    if (m_reconnectHelper->servers().empty())
    {
        disconnectFromServer(DisconnectFlag::Force);
        return;
    }

    m_reconnectHelper->next();
    NX_ASSERT(m_reconnectDialog);
    if (m_reconnectDialog)
        m_reconnectDialog->setCurrentServer(m_reconnectHelper->currentServer());

    if (m_reconnectHelper->currentUrl().isValid())
    {
        connectToServer(m_reconnectHelper->currentUrl());
    }
    else
    {
        executeDelayedParented(
            [this] { reconnectStep(); }, kReconnectDelayMs, m_reconnectHelper.data());
    }
    return;
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened()
{
    NX_ASSERT(m_logicalState != LogicalState::disconnected);
    if (m_logicalState == LogicalState::disconnected)
        return;

    if (m_logicalState == LogicalState::reconnecting)
    {
        /* We can get connectionOpened while testing connection to another server. */
        NX_ASSERT(m_physicalState == PhysicalState::waiting_peer
            || m_physicalState == PhysicalState::testing);
        stopReconnecting();
    }
    setPhysicalState(PhysicalState::waiting_resources);

    action(action::OpenLoginDialogAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM #Common use conditional texts?

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.uuid != commonModule()->moduleGUID())
                return;

            /* We can get here during disconnect process */
            if (auto connection = commonModule()->ec2Connection())
            {
                connection->getMiscManager(Qn::kSystemAccess)->saveRuntimeInfo(
                    info.data,
                    ec2::DummyHandler::instance(),
                    &ec2::DummyHandler::onRequestDone);

            }
        });

    auto connection = commonModule()->ec2Connection();
    NX_ASSERT(connection);
    commonModule()->setReadOnly(connection->connectionInfo().ecDbReadOnly);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed()
{
    NX_ASSERT(commonModule()->ec2Connection());
    disconnect(commonModule()->ec2Connection(), nullptr, this, nullptr);
    disconnect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this, nullptr);

    /* Don't do anything if we are closing client. */
    if (context()->closingDown())
        return;

    switch (m_logicalState)
    {
        case LogicalState::disconnected:
            NX_ASSERT(m_physicalState == PhysicalState::disconnected);
            disconnectFromServer(DisconnectFlag::Force);
            break;

        /* Silently ignoring connection closing when installing updates. */
        case LogicalState::installing_updates:
            setPhysicalState(PhysicalState::waiting_peer);
            break;

        /* Handle reconnect scenario. */
        case LogicalState::reconnecting:
        case LogicalState::connected:
            setPhysicalState(PhysicalState::waiting_peer);
            setLogicalState(LogicalState::reconnecting);
            if (!tryToRestoreConnection())
                disconnectFromServer(DisconnectFlag::Force);
            break;

        /* Connect failed, disconnecting. Whats with videowall? */
        case LogicalState::connecting_to_target:
        case LogicalState::connecting:
            disconnectFromServer(DisconnectFlag::Force);
            break;

        default:
            break;
    }
}

void QnWorkbenchConnectHandler::at_messageProcessor_initialResourcesReceived()
{
    /* Avoid double reconnect when server is very slow or in debug. */
    m_connecting.reset();

    // We can close client while connecting, and initial resources are queued now.
    if (m_logicalState == LogicalState::disconnected)
        return;

    /* We could get here if server advanced settings were changed so peer was reset. */
    if (m_logicalState == LogicalState::connected)
        return;

    NX_ASSERT(m_physicalState == PhysicalState::waiting_resources);
    setState(LogicalState::connected, PhysicalState::connected);

    /* Reload all dialogs and dependent data.
     * It's done delayed because some things - for example qnGlobalSettings -
     * are updated in QueuedConnection to initial notification or resource addition. */
    const auto workbenchStateUpdate =
        [this] { context()->instance<QnWorkbenchStateManager>()->forcedUpdate(); };

    executeLater(workbenchStateUpdate, this);

    /* In several seconds after connect show warnings. */
    executeDelayedParented([this] { showWarnMessagesOnce(); }, kMessagesDelayMs, this);
}

void QnWorkbenchConnectHandler::at_connectAction_triggered()
{
    NX_VERBOSE(this, "Connect to server triggered");
    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setVisibleControls(true);

    bool directConnection = !qnRuntime->isDesktopMode();
    if (m_logicalState == LogicalState::connected)
    {
        // Ask user if he wants to save changes.
        const auto flags = directConnection
            ? DisconnectFlag::Force
            : DisconnectFlag::NoFlags;

        NX_VERBOSE(this, "Disconnecting from the current server first");
        if (!disconnectFromServer(flags))
        {
            NX_VERBOSE(this, "User cancelled the disconnect");
            return;
        }
    }
    else
    {
        // Break 'Connecting' state and clear workbench.
        NX_VERBOSE(this, "Forcefully cleaning the state");
        disconnectFromServer(DisconnectFlag::Force);
    }

    commonModule()->updateRunningInstanceGuid();

    const auto actionParameters = menu()->currentParameters(sender());
    NX_ASSERT(actionParameters.hasArgument(Qn::LogonParametersRole));

    const auto parameters = actionParameters.argument<LogonParameters>(Qn::LogonParametersRole);
    NX_DEBUG(this, "Connecting to the server %1", parameters.url);
    NX_VERBOSE(this,
        "Connection flags: storeSession %1, storePassword %2, autoLogin %3, force %4, secondary %5",
        parameters.storeSession,
        parameters.storePassword,
        parameters.autoLogin,
        parameters.force,
        parameters.secondaryInstance);

    if (directConnection)
    {
        // We don't have to test connection here.
        NX_ASSERT(parameters.url.isValid());
        setLogicalState(LogicalState::connecting_to_target);
        NX_VERBOSE(this, "Directly connecting to the server");
        connectToServer(parameters.url);
    }
    else if (parameters.url.isValid())
    {
        ConnectionOptions options(UpdateSystemWeight);
        if (parameters.storeSession)
            options |= StoreSession;
        if (parameters.storePassword && NX_ASSERT(qnSettings->saveCredentialsAllowed()))
            options |= StorePassword;
        if (parameters.autoLogin && NX_ASSERT(qnSettings->saveCredentialsAllowed()))
            options |= AutoLogin;

        // Ignore warning messages for now if one client instance is opened already.
        if (parameters.secondaryInstance)
            m_warnMessagesDisplayed = true;

        NX_VERBOSE(this, "Connecting to the server with testing before");
        testConnectionToServer(parameters.url, options, parameters.force);
    }
    else if (qnSettings->saveCredentialsAllowed())
    {
        /* Try to load last used connection. */
        NX_DEBUG(this, "Trying to load last saved connection");
        auto url = qnSettings->lastUsedConnection().url;
        const auto localSystemId = qnSettings->lastUsedConnection().localId;

        // Try to connect with saved password. No need to store session once more.
        const bool autoLogin = qnSettings->autoLogin();
        if (autoLogin && url.isValid() && !localSystemId.isNull())
        {
            auto credentials = nx::vms::client::core::helpers::getCredentials(localSystemId,
                url.userName());

            // Try login with cloud user if we do not have an option from the credentials store.
            if (!credentials.isValid() && qnCloudStatusWatcher->credentials().user == url.userName())
                credentials = qnCloudStatusWatcher->credentials();

            if (credentials.isValid())
            {
                url.setPassword(credentials.password);
                NX_DEBUG(this, "Auto-login to the server %1", url);
                NX_VERBOSE(this, "Connecting to the server with testing before");
                testConnectionToServer(url, AutoLogin, /*force*/ true);
            }
        }
    }
}

void QnWorkbenchConnectHandler::at_connectToCloudSystemAction_triggered()
{
    if (!qnCloudStatusWatcher->isCloudEnabled()
        || qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
    {
        return;
    }

    const auto parameters = menu()->currentParameters(sender());
    QString id = parameters.argument(Qn::CloudSystemIdRole).toString();
    NX_DEBUG(this, "Connecting to the cloud system %1", id);

    auto system = qnSystemsFinder->getSystem(id);
    if (!system || !system->isConnectable())
        return;

    auto serverToConnect = nx::vms::client::core::helpers::preferredCloudServer(id);
    if (serverToConnect.isNull() || !system->isReachableServer(serverToConnect))
    {
        const auto servers = system->servers();
        const auto iter = std::find_if(servers.cbegin(), servers.cend(),
            [system](const nx::vms::api::ModuleInformation& server)
            {
                return system->isReachableServer(server.id);
            });

        if (iter != servers.cend())
            serverToConnect = iter->id;
    }

    if (serverToConnect.isNull())
        return;

    nx::utils::Url url = system->getServerHost(serverToConnect);

    auto credentials = qnCloudStatusWatcher->credentials();
    url.setUserName(credentials.user);
    url.setPassword(credentials.password);

    LogonParameters logon(url);
    logon.autoLogin = qnCloudStatusWatcher->stayConnected();

    menu()->trigger(action::ConnectAction,
        action::Parameters().withArgument(Qn::LogonParametersRole, logon));
}

void QnWorkbenchConnectHandler::at_reconnectAction_triggered()
{
    /* Reconnect call should not be executed while we are disconnected. */
    if (m_logicalState != LogicalState::connected)
        return;

    nx::utils::Url currentUrl = commonModule()->currentUrl();
    disconnectFromServer(DisconnectFlag::Force);

    // Do not store connections in case of reconnection
    setLogicalState(LogicalState::connecting_to_target);
    NX_VERBOSE(this, "Reconnecting to the server %1", currentUrl);
    connectToServer(currentUrl);
}

void QnWorkbenchConnectHandler::at_disconnectAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    DisconnectFlags flags = DisconnectFlag::ClearAutoLogin;
    if (parameters.hasArgument(Qn::ForceRole) && parameters.argument(Qn::ForceRole).toBool())
        flags |= DisconnectFlag::Force;

    NX_DEBUG(this, "Disconnecting from the server");
    disconnectFromServer(flags);
}

void QnWorkbenchConnectHandler::at_selectCurrentServerAction_triggered()
{
    if (m_logicalState != LogicalState::connected)
        return;

    const auto parameters = menu()->currentParameters(sender());
    const auto server = parameters.resource().dynamicCast<QnMediaServerResource>();

    if (!NX_ASSERT(server))
        return;

    const auto serverId = server->getId();
    if (!NX_ASSERT(serverId != commonModule()->remoteGUID()))
        return;

    const auto discoveryManager = commonModule()->moduleDiscoveryManager();
    const auto endpoint = discoveryManager->getEndpoint(serverId);
    if (!NX_ASSERT(endpoint && server->isOnline()))
        return;

    nx::utils::Url currentUrl = commonModule()->currentUrl();
    const auto systemId = globalSettings()->cloudSystemId();

    if (!disconnectFromServer(DisconnectFlag::NoFlags))
        return;

    nx::utils::Url url;
    url.setUserName(currentUrl.userName());
    url.setPassword(currentUrl.password());
    url.setPort(endpoint->port);
    url.setHost((systemId.isEmpty() || !hasInternet(server))
        ? endpoint->address.toString()
        : nx::vms::client::core::helpers::serverCloudHost(systemId, serverId));

    setLogicalState(LogicalState::connecting_to_target);
    m_connecting.storeConnection = true;
    connectToServer(url);
}

void QnWorkbenchConnectHandler::connectToServer(const nx::utils::Url &url)
{
    auto validState =
        m_logicalState == LogicalState::testing
        || m_logicalState == LogicalState::connecting
        || m_logicalState == LogicalState::connecting_to_target
        || m_logicalState == LogicalState::reconnecting;
    NX_ASSERT(validState);
    if (!validState)
        return;

    m_connecting.url = url;
    if (ini().forceJsonClientConnection)
    {
        QUrlQuery query(m_connecting.url.toQUrl());
        query.removeQueryItem("format");
        query.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
        m_connecting.url.setQuery(query);
    }

    setPhysicalState(PhysicalState::testing);
    NX_VERBOSE(this, "Executing connect to the %1", m_connecting.url);
    m_connecting.handle = qnClientCoreModule->connectionFactory()->connect(
        m_connecting.url, clientInfo(), this, &QnWorkbenchConnectHandler::handleConnectReply);

}

void QnWorkbenchConnectHandler::connectToServerForUpdate(const nx::utils::Url &url)
{
    auto validState =
        m_logicalState == LogicalState::testing
        || m_logicalState == LogicalState::connecting
        || m_logicalState == LogicalState::connecting_to_target
        || m_logicalState == LogicalState::reconnecting;
    NX_ASSERT(validState);
    if (!validState)
        return;

    setPhysicalState(PhysicalState::pulling_updates);
    m_connecting.handle = qnClientCoreModule->connectionFactory()->connect(
        url, clientInfo(), this, &QnWorkbenchConnectHandler::handleConnectReply);
    m_connecting.url = url;
}

bool QnWorkbenchConnectHandler::disconnectFromServer(DisconnectFlags flags)
{
    const bool force = flags.testFlag(DisconnectFlag::Force);
    const bool isErrorReason = flags.testFlag(DisconnectFlag::ErrorReason);

    if (flags.testFlag(DisconnectFlag::ClearAutoLogin))
    {
        qnSettings->setAutoLogin(false);
        qnSettings->save();
    }

    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (!force)
        qnGlobalSettings->synchronizeNow();

    if (flags.testFlag(SessionTimeout))
    {
        executeDelayedParented([this]() { SessionExpiredDialog::exec(context()->mainWindowWidget()); },
            this);
    }

    if (isErrorReason && mainWindow()->welcomeScreen())
        mainWindow()->welcomeScreen()->openConnectingTile();

    setState(LogicalState::disconnected, PhysicalState::disconnected);

    clearConnection();
    return true;
}

void QnWorkbenchConnectHandler::handleTestConnectionReply(int handle,
    const nx::utils::Url &url,
    ec2::ErrorCode errorCode,
    const QnConnectionInfo& connectionInfo,
    ConnectionOptions options,
    bool force)
{
    const bool invalidState = ((m_logicalState != LogicalState::testing)
        && (m_logicalState != LogicalState::connecting_to_target));

    if (m_connecting.handle != handle || invalidState)
        return;

    m_connecting.reset();

    /* Preliminary exit if application was closed while we were in the inner loop. */
    NX_ASSERT(!context()->closingDown());
    if (context()->closingDown())
        return;

    // TODO: Should enter 'updating' mode

    const auto status =  QnConnectionDiagnosticsHelper::validateConnection(
        connectionInfo, errorCode, mainWindowWidget(), commonModule()->engineVersion());
    NX_VERBOSE(this, "Testing connection to the %1: result %2", url, status);

    switch (status)
    {
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
        // This code is also returned if we are downloading compatibility version
        case Qn::IncompatibleVersionConnectionResult:
            // Do not store connection if applauncher is offline
            if (!nx::vms::applauncher::api::checkOnline(false))
                break;
            // Fall through
        case Qn::SuccessConnectionResult:
            storeConnectionRecord(url, connectionInfo, options);
            break;
        default:
            break;
    }

    switch (status)
    {
        case Qn::SuccessConnectionResult:
            setLogicalState(force ? LogicalState::connecting : LogicalState::connecting_to_target);
            connectToServer(url);
            break;
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
            menu()->trigger(action::DelayedForcedExitAction);
            break;
        default:
            disconnectFromServer(DisconnectFlag::Force | DisconnectFlag::ErrorReason);
            break;
    }
}

void QnWorkbenchConnectHandler::showLoginDialog()
{
    if (context()->closingDown())
        return;

    const QScopedPointer<QnLoginDialog> dialog(new QnLoginDialog(mainWindow()));
    dialog->exec();
}

void QnWorkbenchConnectHandler::clearConnection()
{
    stopReconnecting();

    qnClientMessageProcessor->init(nullptr);
    QnAppServerConnectionFactory::setEc2Connection(nullptr);

    commonModule()->sessionManager()->stop();
    context()->setUserName(QString());

    /* Get ready for the next connection. */
    m_warnMessagesDisplayed = false;

    action(action::OpenLoginDialogAction)->setText(tr("Connect to Server..."));

    /* Remove all remote resources. */
    auto resourcesToRemove = resourcePool()->getResourcesWithFlag(Qn::remote);

    /* Also remove layouts that were just added and have no 'remote' flag set. */
    for (const auto& layout : resourcePool()->getResources<QnLayoutResource>())
    {
        if (layout->hasFlags(Qn::local) && !layout->isFile())  //do not remove exported layouts
            resourcesToRemove.push_back(layout);
    }

    for (const auto aviResource: resourcePool()->getResources<QnAviResource>())
    {
        if (!aviResource->isOnline() && !aviResource->isEmbedded())
            resourcesToRemove.push_back(aviResource);
    }

    for (const auto fileLayoutResource: resourcePool()->getResources<QnFileLayoutResource>())
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

    resourcePool()->removeResources(resourcesToRemove);

    cameraUserAttributesPool()->clear();
    mediaServerUserAttributesPool()->clear();
    resourcePropertyDictionary()->clear(idList);
    statusDictionary()->clear(idList);

    licensePool()->reset();
    commonModule()->setReadOnly(false);

    resourceAccessProvider()->endUpdate();
    resourceAccessManager()->endUpdate();
}

void QnWorkbenchConnectHandler::testConnectionToServer(
    const nx::utils::Url& url,
    ConnectionOptions options,
    bool force)
{
    setLogicalState(force ? LogicalState::connecting_to_target : LogicalState::testing);

    setPhysicalState(PhysicalState::testing);
    m_connecting.handle = qnClientCoreModule->connectionFactory()->testConnection(
        url, this,
        [this, options, url, force]
        (int handle, ec2::ErrorCode errorCode, const QnConnectionInfo& connectionInfo)
        {
            handleTestConnectionReply(handle, url, errorCode, connectionInfo, options, force);
        });
    m_connecting.url = url;
}

bool QnWorkbenchConnectHandler::tryToRestoreConnection()
{
    using nx::vms::client::core::ReconnectHelper;

    nx::utils::Url currentUrl = commonModule()->currentUrl();
    NX_ASSERT(!currentUrl.isEmpty());
    if (currentUrl.isEmpty())
        return false;

    if (!m_reconnectHelper)
        m_reconnectHelper.reset(new ReconnectHelper(qnSettings->stickReconnectToServer()));

    if (m_reconnectHelper->servers().isEmpty())
    {
        stopReconnecting();
        return false;
    }

    m_reconnectDialog = new QnReconnectInfoDialog(mainWindowWidget());
    m_reconnectDialog->setCurrentServer(m_reconnectHelper->currentServer());
    connect(m_reconnectDialog, &QDialog::rejected, this,
        [this]
        {
            stopReconnecting();
            disconnectFromServer(DisconnectFlag::Force);
            if (!qnRuntime->isDesktopMode())
                menu()->trigger(action::DelayedForcedExitAction);
        });
    QnDialog::show(m_reconnectDialog);

    reconnectStep();
    return true;
}
