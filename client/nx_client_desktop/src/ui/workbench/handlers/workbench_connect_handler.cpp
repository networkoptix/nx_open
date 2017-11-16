
#include "workbench_connect_handler.h"

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
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/media_server_resource.h>

#include <client_core/client_core_settings.h>
#include <client/desktop_client_message_processor.h>

#include <finders/systems_finder.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>

#include <helpers/system_weight_helper.h>
#include <nx_ec/ec_proto_version.h>
#include <llutil/hardware_id.h>

#include <platform/hardware_information.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

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

#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/workbench_license_notifier.h>

#include <utils/applauncher_utils.h>
#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/common/app_info.h>

#include <nx/utils/collection.h>
#include <utils/common/synctime.h>
#include <utils/common/system_information.h>
#include <utils/common/url.h>
#include <utils/common/delayed.h>
#include <nx/vms/discovery/manager.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <utils/reconnect_helper.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <helpers/system_helpers.h>

#include <watchers/cloud_status_watcher.h>
#include <nx_ec/dummy_handler.h>


using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

static const int kVideowallCloseTimeoutMSec = 10000;
static const int kMessagesDelayMs = 5000;
static constexpr int kReconnectDelayMs = 3000;

bool isConnectionToCloud(const nx::utils::Url& url)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host());
}

bool isSameConnectionUrl(const nx::utils::Url& first, const nx::utils::Url& second)
{
    return ((first.host() == second.host())
        && (first.port() == second.port())
        && (first.userName() == second.userName()));
}

QString getConnectionName(const QString& systemName, const nx::utils::Url& url)
{
    static const auto kNameTemplate = QnWorkbenchConnectHandler::tr("%1 in %2",
        "%1 is user name, %2 is name of system");

    return kNameTemplate.arg(url.userName(), systemName);
}

void removeCustomConnection(const QnUuid& localId, const nx::utils::Url& url)
{
    NX_ASSERT(!localId.isNull(), "We can't remove custom user connections");

    auto customConnections = qnSettings->customConnections();

    const auto itSameSystem = std::find_if(customConnections.begin(), customConnections.end(),
        [&localId, user = url.userName()](const QnConnectionData& value)
        {
            return localId == value.localId && value.url.userName() == user;
        });

    if (itSameSystem == customConnections.end())
        return;

    customConnections.erase(itSameSystem);
    qnSettings->setCustomConnections(customConnections);
    qnSettings->save();
}

void storeCustomConnection(const QnUuid& localId, const QString& systemName, const nx::utils::Url& url)
{
    if (url.password().isEmpty())
        return;

    NX_ASSERT(!localId.isNull(), "We can't remove custom user connections");

    auto customConnections = qnSettings->customConnections();

    const auto itSameUrl = std::find_if(customConnections.begin(), customConnections.end(),
        [&url](const QnConnectionData& value)
        {
            return isSameConnectionUrl(url, value.url);
        });

    const bool sameUrlFound = (itSameUrl != customConnections.end());
    if (sameUrlFound && (itSameUrl->url.password() == url.password()))
        return; // We don't add/update stored connection with existing url and same password

    if (sameUrlFound)
        itSameUrl->url = url;

    const auto itSameSystem = std::find_if(customConnections.begin(), customConnections.end(),
        [&localId, userName = url.userName()](const QnConnectionData& value)
        {
            return localId == value.localId && value.url.userName() == userName;
        });

    const bool sameSystemFound = (itSameSystem != customConnections.end());
    if (sameSystemFound)
        itSameSystem->url = url;

    if (!sameSystemFound && !sameUrlFound)
    {
        // Adds new stored connection
        auto connectionName = getConnectionName(systemName, url);
        if (customConnections.contains(connectionName))
            connectionName = customConnections.generateUniqueName(connectionName);

        customConnections.append(QnConnectionData(connectionName, url, localId));
    }

    qnSettings->setCustomConnections(customConnections);
    qnSettings->save();
}

void storeLocalSystemConnection(
    const QString& systemName,
    const QnUuid& localSystemId,
    nx::utils::Url url,
    bool storePassword)
{
    if (!storePassword)
        url.setPassword(QString());

    using namespace nx::client::core::helpers;

    storeConnection(localSystemId, systemName, url);

    qnClientCoreSettings->save();

    if (storePassword)
        storeCustomConnection(localSystemId, systemName, url);
    else
        removeCustomConnection(localSystemId, url);

    qnSettings->save();
}

ec2::ApiClientInfoData clientInfo()
{
    ec2::ApiClientInfoData clientData;
    clientData.id = qnSettings->pcUuid();
    clientData.fullVersion = nx::utils::AppInfo::applicationFullVersion();
    clientData.systemInfo = QnSystemInformation::currentSystemInformation().toString();
    clientData.systemRuntime = QnSystemInformation::currentSystemRuntime();

    const auto& hw = HardwareInformation::instance();
    clientData.physicalMemory = hw.physicalMemory;
    clientData.cpuArchitecture = hw.cpuArchitecture;
    clientData.cpuModelName = hw.cpuModelName;

    const auto gl = QnGlFunctions::openGLCachedInfo();
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
    m_logicalState(LogicalState::disconnected),
    m_physicalState(PhysicalState::disconnected),
    m_warnMessagesDisplayed(false),
    m_crashReporter(commonModule())
{
    connect(this, &QnWorkbenchConnectHandler::stateChanged, this,
        &QnWorkbenchConnectHandler::handleStateChanged);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_initialResourcesReceived);

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
}

QnWorkbenchConnectHandler::~QnWorkbenchConnectHandler()
{
}

void QnWorkbenchConnectHandler::handleConnectReply(
    int handle,
    ec2::ErrorCode errorCode,
    ec2::AbstractECConnectionPtr connection)
{
    /* Check if we have entered 'connect' method again while were in 'connecting...' state */
    if (m_connecting.handle != handle)
    {
        NX_LOG("handleConnectReply: waiting for another request, ignore", cl_logDEBUG1);
        return;
    }

    if (m_logicalState == LogicalState::disconnected)
    {
        NX_LOG("handleConnectReply: already disconnected, ignore", cl_logDEBUG1);
        return;
    }

    if (m_physicalState != PhysicalState::testing)
    {
        NX_LOG(lm("handleConnectReply: invalid physical state %1").arg(physicalToString(m_physicalState)), cl_logDEBUG1);
        return;
    }

    m_connecting.reset();

    /* Preliminary exit if application was closed while we were in the inner loop. */
    NX_ASSERT(!context()->closingDown());
    if (context()->closingDown())
    {
        NX_LOG("handleConnectReply: closing application", cl_logDEBUG1);
        return;
    }

    NX_ASSERT(connection || errorCode != ec2::ErrorCode::ok);
    QnConnectionInfo connectionInfo;
    if (connection)
        connectionInfo = connection->connectionInfo();

    const bool silent = m_logicalState == LogicalState::reconnecting
        || !qnRuntime->isDesktopMode();

    auto status = silent
        ? QnConnectionValidator::validateConnection(connectionInfo, errorCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errorCode, mainWindowWidget());
    NX_ASSERT(connection || status != Qn::SuccessConnectionResult);
    NX_LOG(lm("handleConnectReply: connection status %1").arg(status), cl_logDEBUG1);

    if (m_logicalState == LogicalState::reconnecting)
    {
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
            }
            break;
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
            menu()->trigger(action::DelayedForcedExitAction);
            break;
        default:    //error
            if (!qnRuntime->isDesktopMode())
            {
                QnGraphicsMessageBox::information(
                        tr("Could not connect to server. Video Wall will be closed."),
                        kVideowallCloseTimeoutMSec);
                executeDelayedParented(
                    [this]
                    {
                        action(action::ExitAction)->trigger();
                    }, kVideowallCloseTimeoutMSec, this
                );
            }
            else
            {
                disconnectFromServer(static_cast<DisconnectFlags>(
                    DisconnectFlag::Force | DisconnectFlag::ErrorReason));
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
    QnResource::startCommandProc();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
        ? url.userName()
        : connectionInfo.effectiveUserName);
}

void QnWorkbenchConnectHandler::storeConnectionRecord(
    const nx::utils::Url& url,
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
    nx::client::core::helpers::updateWeightData(localId);

    const bool cloudConnection = isConnectionToCloud(url);

    // Stores local credentials for successful connection
    if (helpers::isLocalUser(url.userName()) && !cloudConnection && options.testFlag(StoreSession))
    {
        NX_DEBUG(nx::client::core::helpers::kCredentialsLogTag,
            lm("Store connection record of %1 to the system %2").args(url.userName(), localId));

        NX_ASSERT(!url.password().isEmpty());
        NX_DEBUG(nx::client::core::helpers::kCredentialsLogTag, storePassword
            ? "Password is set"
            : "Password must be cleared");

        const auto credentials = (storePassword
            ? QnEncodedCredentials(url)
            : QnEncodedCredentials(url.userName(), QString()));

        nx::client::core::helpers::storeCredentials(localId, credentials);
        qnClientCoreSettings->save();
    }

    if (autoLogin)
    {
        NX_ASSERT(!url.password().isEmpty());
        NX_DEBUG(nx::client::core::helpers::kCredentialsLogTag,
            lm("Saving last used connection of %1 to the system %2").args(url.userName(), url.host()));

        const auto lastUsed = QnConnectionData(info.systemName, url, localId);
        qnSettings->setLastUsedConnection(lastUsed);
    }

    qnSettings->setAutoLogin(autoLogin);
    qnSettings->save();

    if (cloudConnection)
    {
        qnCloudStatusWatcher->logSession(info.cloudSystemId);
    }
    else
    {
        NX_ASSERT(!url.host().isEmpty(), "Wrong host is going to be saved to the recent connections list");

        // Stores connection if it is local
        storeLocalSystemConnection(info.systemName, localId, url, storePassword);
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

    auto watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
    if (watcher->hasMismatches())
        menu()->trigger(action::VersionMismatchMessageAction);       

    context()->instance<QnWorkbenchLicenseNotifier>()->checkLicenses();
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

    NX_DEBUG(this) << "State changed" << logicalValue << physicalValue;
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
    NX_EXPECT(m_reconnectDialog);
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
    connect(connection->getTimeNotificationManager(),
        &ec2::AbstractTimeNotificationManager::timeChanged,
        this,
        [](qint64 syncTime)
        {
            NX_ASSERT(qnSyncTime);
            if (qnSyncTime)
                qnSyncTime->updateTime(syncTime);
        });

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

    NX_ASSERT(m_logicalState != LogicalState::disconnected);
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

    executeDelayedParented(workbenchStateUpdate, 0, this);

    /* In several seconds after connect show warnings. */
    executeDelayedParented([this] { showWarnMessagesOnce(); }, kMessagesDelayMs, this);
}

void QnWorkbenchConnectHandler::at_connectAction_triggered()
{
    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setVisibleControls(true);

    bool directConnection = !qnRuntime->isDesktopMode();
    if (m_logicalState == LogicalState::connected)
    {
        // Ask user if he wants to save changes.
        const auto flags = directConnection
            ? DisconnectFlag::Force
            : DisconnectFlag::NoFlags;

        if (!disconnectFromServer(flags))
            return;
    }
    else
    {
        // Break 'Connecting' state and clear workbench.
        disconnectFromServer(DisconnectFlag::Force);
    }

    commonModule()->updateRunningInstanceGuid();

    const auto parameters = menu()->currentParameters(sender());
    NX_ASSERT(parameters.hasArgument(Qn::StoreSessionRole));
    const bool storeSession = parameters.argument(Qn::StoreSessionRole, true);
    nx::utils::Url url = parameters.argument(Qn::UrlRole, nx::utils::Url());

    if (directConnection)
    {
        // We don't have to test connection here.
        NX_ASSERT(url.isValid());
        setLogicalState(LogicalState::connecting_to_target);
        connectToServer(url);
    }
    else if (url.isValid())
    {
        const auto forceConnection = parameters.argument(Qn::ForceRole, false);
        ConnectionOptions options;
        if (storeSession)
            options |= StoreSession;
        if (parameters.argument(Qn::StorePasswordRole, false))
            options |= StorePassword;
        if (parameters.argument(Qn::AutoLoginRole, false))
            options |= AutoLogin;

        testConnectionToServer(url, options, forceConnection);
    }
    else
    {
        /* Try to load last used connection. */
        url = qnSettings->lastUsedConnection().url;

        // Try to connect with saved password. No need to store session once more.
        const bool autoLogin = qnSettings->autoLogin();
        if (autoLogin && url.isValid() && !url.password().isEmpty())
            testConnectionToServer(url, AutoLogin, true);
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

    auto system = qnSystemsFinder->getSystem(id);
    if (!system || !system->isConnectable())
        return;

    const auto servers = system->servers();
    auto reachableServer = std::find_if(servers.cbegin(), servers.cend(),
        [system](const QnModuleInformation& server)
        {
            return system->isReachableServer(server.id);
        });

    if (reachableServer == servers.cend())
        return;

    nx::utils::Url url = system->getServerHost(reachableServer->id);
    auto credentials = qnCloudStatusWatcher->credentials();
    url.setUserName(credentials.user);
    url.setPassword(credentials.password.value());

    action::Parameters connectParams{Qn::UrlRole, url};
    connectParams.setArgument(Qn::StoreSessionRole, true);
    menu()->trigger(action::ConnectAction, connectParams);
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
    connectToServer(currentUrl);
}

void QnWorkbenchConnectHandler::at_disconnectAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const DisconnectFlags flags = static_cast<DisconnectFlags>(
        parameters.hasArgument(Qn::ForceRole) && parameters.argument(Qn::ForceRole).toBool()
            ? DisconnectFlag::Force | DisconnectFlag::ClearAutoLogin
            : DisconnectFlag::ClearAutoLogin);

    disconnectFromServer(flags);
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

    setPhysicalState(PhysicalState::testing);
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

    auto status =  QnConnectionDiagnosticsHelper::validateConnection(
        connectionInfo, errorCode, mainWindowWidget());

    switch (status)
    {
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
        // This code is also returned if we are downloading compatibility version
        case Qn::IncompatibleVersionConnectionResult:
            // Do not store connection if applauncher is offline
            if (!applauncher::checkOnline(false))
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
            disconnectFromServer(static_cast<DisconnectFlags>(
                DisconnectFlag::Force | DisconnectFlag::ErrorReason));
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
    QnResource::stopCommandProc();

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

    resourceAccessManager()->beginUpdate();
    resourceAccessProvider()->beginUpdate();

    QVector<QnUuid> idList;
    idList.reserve(resourcesToRemove.size());
    for (const auto& res: resourcesToRemove)
        idList.push_back(res->getId());

    resourcePool()->removeResources(resourcesToRemove);

    cameraUserAttributesPool()->clear();
    mediaServerUserAttributesPool()->clear();
    propertyDictionary()->clear(idList);
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
    nx::utils::Url currentUrl = commonModule()->currentUrl();
    NX_ASSERT(!currentUrl.isEmpty());
    if (currentUrl.isEmpty())
        return false;

    if (!m_reconnectHelper)
        m_reconnectHelper.reset(new QnReconnectHelper());

    if (m_reconnectHelper->servers().isEmpty())
    {
        stopReconnecting();
        return false;
    }

    m_reconnectDialog = new QnReconnectInfoDialog(mainWindowWidget());
    connect(m_reconnectDialog, &QDialog::rejected, this,
        [this]
        {
            stopReconnecting();
            disconnectFromServer(DisconnectFlag::Force);
        });
    m_reconnectDialog->setServers(m_reconnectHelper->servers());
    QnDialog::show(m_reconnectDialog);

    reconnectStep();
    return true;
}
