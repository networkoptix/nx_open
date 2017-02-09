
#include "workbench_connect_handler.h"

#include <QtNetwork/QHostInfo>

#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/media_server_resource.h>

#include <client_core/client_core_settings.h>
#include <client/desktop_client_message_processor.h>

#include <finders/systems_finder.h>

#include <nx/network/socket_global.h>

#include <helpers/system_weight_helper.h>
#include <nx_ec/ec_proto_version.h>
#include <llutil/hardware_id.h>

#include <platform/hardware_information.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>

#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/items/resource/resource_widget.h>

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

#include <utils/applauncher_utils.h>
#include <utils/app_server_notification_cache.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/common/app_info.h>

#include <nx/utils/collection.h>
#include <utils/common/synctime.h>
#include <utils/common/system_information.h>
#include <utils/common/url.h>
#include <utils/common/delayed.h>
#include <network/module_finder.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <utils/reconnect_helper.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <helpers/system_helpers.h>

#include <watchers/cloud_status_watcher.h>

namespace {

static const int kVideowallCloseTimeoutMSec = 10000;
static const int kMessagesDelayMs = 5000;

bool isConnectionToCloud(const QUrl& url)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host());
}

bool isSameConnectionUrl(const QUrl& first, const QUrl& second)
{
    return ((first.host() == second.host())
        && (first.port() == second.port())
        && (first.userName() == second.userName()));
}

QString getConnectionName(const QString& systemName, const QUrl& url)
{
    static const auto kNameTemplate = QnWorkbenchConnectHandler::tr("%1 in %2",
        "%1 is user name, %2 is name of system");

    return kNameTemplate.arg(url.userName(), systemName);
}

void removeCustomConnection(const QnUuid& localId, const QUrl& url)
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

void storeCustomConnection(const QnUuid& localId, const QString& systemName, const QUrl& url)
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
    QUrl url,
    bool storePassword,
    bool autoLogin)
{
    if (autoLogin)
        storePassword = true;

    if (!storePassword)
        url.setPassword(QString());

    using namespace nx::client::core::helpers;

    storeConnection(localSystemId, systemName, url);

    qnClientCoreSettings->save();

    const auto lastUsed = QnConnectionData(systemName, url, localSystemId);
    qnSettings->setLastUsedConnection(lastUsed);
    qnSettings->setAutoLogin(autoLogin);

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

ec2::AbstractECConnectionPtr connection2()
{
    return QnAppServerConnectionFactory::getConnection2();
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
    m_warnMessagesDisplayed(false)
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
            QnPeerRuntimeInfo localInfo = qnRuntimeInfoManager->localInfo();
            localInfo.data.userId = user ? user->getId() : QnUuid();
            qnRuntimeInfoManager->updateLocalItem(localInfo);
        });

    connect(action(QnActions::ConnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(QnActions::ConnectToCloudSystemAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_connectToCloudSystemAction_triggered);
    connect(action(QnActions::ReconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(QnActions::DisconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(QnActions::OpenLoginDialogAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::showLoginDialog, Qt::QueuedConnection);
    connect(action(QnActions::BeforeExitAction), &QAction::triggered, this,
        [this]
        {
            disconnectFromServer(true);
        });

    connect(action(QnActions::LogoutFromCloud), &QAction::triggered, this,
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
                        disconnectFromServer(true, true);
                    return;
                case LogicalState::connected:
                case LogicalState::reconnecting:
                    break;
                default:
                    NX_ASSERT(false, "Unhandled connection state");
            }


            /* Check if we need to log out if logged in under this user. */
            QString currentLogin = QnAppServerConnectionFactory::url().userName();
            NX_ASSERT(!currentLogin.isEmpty());
            if (currentLogin.isEmpty())
                return;

            if (qnCloudStatusWatcher->effectiveUserName() == currentLogin)
                disconnectFromServer(true);
        });

    context()->instance<QnAppServerNotificationCache>();

    const auto resourceModeAction = action(QnActions::ResourcesModeAction);
    connect(resourceModeAction, &QAction::toggled, this,
        [this, welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>()](bool checked)
        {
            welcomeScreen->setVisible(!checked);
            if (workbench()->layouts().isEmpty())
                action(QnActions::OpenNewTabAction)->trigger();
        });

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
        return;

    if (m_logicalState == LogicalState::disconnected)
        return;

    if (m_physicalState != PhysicalState::testing)
        return;

    m_connecting.reset();

    /* Preliminary exit if application was closed while we were in the inner loop. */
    NX_ASSERT(!context()->closingDown());
    if (context()->closingDown())
        return;

    NX_ASSERT(connection || errorCode != ec2::ErrorCode::ok);
    QnConnectionInfo connectionInfo;
    if (connection)
        connectionInfo = connection->connectionInfo();

    const bool silent = m_logicalState == LogicalState::reconnecting
        || !qnRuntime->isDesktopMode();

    auto status = silent
        ? QnConnectionValidator::validateConnection(connectionInfo, errorCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errorCode, mainWindow());
    NX_ASSERT(connection || status != Qn::SuccessConnectionResult);

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
                disconnectFromServer(true);
                auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
                /* Method is called from QML where we are passing QString. */
                welcomeScreen->setupFactorySystem(connectionInfo.effectiveUrl().toString());
            }
            else
            {
                establishConnection(connection);
            }
            break;
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
            menu()->trigger(QnActions::DelayedForcedExitAction);
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
                        action(QnActions::ExitAction)->trigger();
                    }, kVideowallCloseTimeoutMSec, this
                );
            }
            else
            {
                disconnectFromServer(true, true);
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
        disconnectFromServer(true);
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
        case Qn::UnauthorizedConnectionResult:
            /* Server database was cleaned up during restart, e.g. merge to other system. */
            m_reconnectHelper->markServerAsInvalid(m_reconnectHelper->currentServer());
            break;
        case Qn::IncompatibleInternalConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
        case Qn::IncompatibleVersionConnectionResult:
        case Qn::IncompatibleProtocolConnectionResult:
            m_reconnectHelper->markServerAsInvalid(m_reconnectHelper->currentServer());
            break;
        default:
            break;
    }

    /* Find next valid server for reconnect. */
    QnMediaServerResourceList allServers = m_reconnectHelper->servers();
    bool found = true;
    do
    {
        m_reconnectHelper->next();
        /* We have found at least one correct interface for the server. */
        found = m_reconnectHelper->currentUrl().isValid();
        if (!found) /* Do not try to connect to invalid servers. */
            allServers.removeOne(m_reconnectHelper->currentServer());
    } while (!found && !allServers.isEmpty());

    /* Break cycle if we cannot find any valid server. */
    if (found)
        connectToServer(m_reconnectHelper->currentUrl());
    else
        disconnectFromServer(true);
}

void QnWorkbenchConnectHandler::establishConnection(ec2::AbstractECConnectionPtr connection)
{
    NX_ASSERT(connection);
    if (!connection)
    {
        disconnectFromServer(true);
        return;
    }

    auto connectionInfo = connection->connectionInfo();

    setPhysicalState(PhysicalState::waiting_peer);
    QUrl url = connectionInfo.effectiveUrl();
    QnAppServerConnectionFactory::setUrl(url);
    QnAppServerConnectionFactory::setEc2Connection(connection);
    QnClientMessageProcessor::instance()->init(connection);

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
        ? url.userName()
        : connectionInfo.effectiveUserName);
}

void QnWorkbenchConnectHandler::storeConnectionRecord(
    const QUrl& url,
    const QnConnectionInfo& info,
    ConnectionOptions options)
{
    /**
     * Note! We don't save connection to cloud or new systems. But we have to update
     * weights for any connection using its local id
     *
     * Also, we always store connection if StorePassword flag is set because it means
     * it is not initial connection to factory system.
     */

    const bool storePassword = options.testFlag(StorePassword);
    if (!storePassword && helpers::isNewSystem(info))
        return;

    const auto localId = helpers::getLocalSystemId(info);
    nx::client::core::helpers::updateWeightData(localId);

    // Stores local credentials for successful connection
    const bool autoLogin = options.testFlag(AutoLogin);
    if (helpers::isLocalUser(url.userName()))
    {
        const auto credentials = (storePassword || autoLogin
            ? QnCredentials(url)
            : QnCredentials(url.userName(), QString()));

        nx::client::core::helpers::storeCredentials(localId, credentials);
        qnClientCoreSettings->save();
    }

    if (options.testFlag(IsCloudConnection))
    {
        using namespace nx::network;
        qnCloudStatusWatcher->logSession(info.cloudSystemId);
        return;
    }

    // Stores connection if it is local
    storeLocalSystemConnection(
        info.systemName,
        localId,
        url,
        storePassword,
        autoLogin
    );
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

    menu()->triggerIfPossible(QnActions::AllowStatisticsReportMessageAction);

    auto watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
    if (!watcher->hasMismatches())
        return;

    menu()->trigger(QnActions::VersionMismatchMessageAction);
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
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
    const auto resourceModeAction = action(QnActions::ResourcesModeAction);

    resourceModeAction->setChecked(false); //< Shows welcome screen
    welcomeScreen->handleConnectingToSystem();
    welcomeScreen->setGlobalPreloaderVisible(true);
}

void QnWorkbenchConnectHandler::handleStateChanged(LogicalState logicalValue,
    PhysicalState physicalValue)
{
    const auto resourceModeAction = action(QnActions::ResourcesModeAction);
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();

    qDebug() << "QnWorkbenchConnectHandler state changed" << logicalValue << physicalValue;
    switch (logicalValue)
    {
        case LogicalState::disconnected:
            welcomeScreen->handleDisconnectedFromSystem();
            welcomeScreen->setGlobalPreloaderVisible(false);
            resourceModeAction->setChecked(false);  //< Shows welcome screen
            break;

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

    action(QnActions::OpenLoginDialogAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM #Common use conditional texts?

    connect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.uuid != qnCommon->moduleGUID())
                return;

            /* We can get here during disconnect process */
            if (auto connection = connection2())
                connection->sendRuntimeData(info.data);
        });


    auto connection = connection2();
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

    qnCommon->setReadOnly(connection->connectionInfo().ecDbReadOnly);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed()
{
    NX_ASSERT(connection2());
    disconnect(connection2(), nullptr, this, nullptr);
    disconnect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, this, nullptr);

    /* Don't do anything if we are closing client. */
    if (context()->closingDown())
        return;

    switch (m_logicalState)
    {
        case LogicalState::disconnected:
            NX_ASSERT(m_physicalState == PhysicalState::disconnected);
            disconnectFromServer(true);
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
                disconnectFromServer(true);
            break;

        /* Connect failed, disconnecting. Whats with videowall? */
        case LogicalState::connecting_to_target:
        case LogicalState::connecting:
            disconnectFromServer(true);
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

    /* Reload all dialogs and dependent data. */
    context()->instance<QnWorkbenchStateManager>()->forcedUpdate();

    /* In several seconds after connect show warnings. */
    executeDelayedParented([this] { showWarnMessagesOnce(); }, kMessagesDelayMs, this);
}

void QnWorkbenchConnectHandler::at_connectAction_triggered()
{
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
    welcomeScreen->setVisibleControls(true);

    bool directConnection = qnRuntime->isActiveXMode() || qnRuntime->isVideoWallMode();
    if (m_logicalState == LogicalState::connected)
    {
        // Ask user if he wants to save changes.
        if (!disconnectFromServer(directConnection))
            return;
    }
    else
    {
        // Break 'Connecting' state and clear workbench.
        disconnectFromServer(true);
    }

    qnCommon->updateRunningInstanceGuid();

    QnActionParameters parameters = menu()->currentParameters(sender());
    QUrl url = parameters.argument(Qn::UrlRole, QUrl());

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
        if (isConnectionToCloud(url))
            options |= IsCloudConnection;
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

        /* Try to connect with saved password. */
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

    QnActionParameters parameters = menu()->currentParameters(sender());
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

    QUrl url = system->getServerHost(reachableServer->id);
    auto credentials = qnCloudStatusWatcher->credentials();
    url.setUserName(credentials.user);
    url.setPassword(credentials.password.value());

    menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, url));
}

void QnWorkbenchConnectHandler::at_reconnectAction_triggered()
{
    /* Reconnect call should not be executed while we are disconnected. */
    if (m_logicalState != LogicalState::connected)
        return;

    QUrl currentUrl = QnAppServerConnectionFactory::url();
    disconnectFromServer(true);

    // Do not store connections in case of reconnection
    setLogicalState(LogicalState::connecting_to_target);
    connectToServer(currentUrl);
}

void QnWorkbenchConnectHandler::at_disconnectAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    bool force = parameters.hasArgument(Qn::ForceRole)
        ? parameters.argument(Qn::ForceRole).toBool()
        : false;

    if (!disconnectFromServer(force))
        return;

    qnSettings->setAutoLogin(false);
    qnSettings->save();
}

void QnWorkbenchConnectHandler::connectToServer(const QUrl &url)
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
    m_connecting.handle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
        url, clientInfo(), this, &QnWorkbenchConnectHandler::handleConnectReply);
    m_connecting.url = url;
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force, bool isErrorReason)
{
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (!force)
        QnGlobalSettings::instance()->synchronizeNow();

    if (isErrorReason && qnRuntime->isDesktopMode())
    {
        const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
        welcomeScreen->openConnectingTile();
    }

    setState(LogicalState::disconnected, PhysicalState::disconnected);

    clearConnection();
    return true;
}

void QnWorkbenchConnectHandler::handleTestConnectionReply(
    int handle,
    const QUrl& url,
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
        connectionInfo, errorCode, mainWindow());

    switch (status)
    {
        case Qn::IncompatibleProtocolConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
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
            menu()->trigger(QnActions::DelayedForcedExitAction);
            break;
        default:
            disconnectFromServer(true, true);
            break;
    }
}

void QnWorkbenchConnectHandler::showLoginDialog()
{
    if (context()->closingDown())
        return;

    const QScopedPointer<QnLoginDialog> dialog(new QnLoginDialog(context()->mainWindow()));
    dialog->exec();
}

void QnWorkbenchConnectHandler::clearConnection()
{
    stopReconnecting();

    qnClientMessageProcessor->init(nullptr);
    QnAppServerConnectionFactory::setEc2Connection(nullptr);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnSessionManager::instance()->stop();
    QnResource::stopCommandProc();

    context()->setUserName(QString());

    /* Get ready for the next connection. */
    m_warnMessagesDisplayed = false;

    action(QnActions::OpenLoginDialogAction)->setText(tr("Connect to Server..."));

    /* Remove all remote resources. */
    auto resourcesToRemove = qnResPool->getResourcesWithFlag(Qn::remote);

    /* Also remove layouts that were just added and have no 'remote' flag set. */
    for (const auto& layout : qnResPool->getResources<QnLayoutResource>())
    {
        if (layout->hasFlags(Qn::local) && !layout->isFile())  //do not remove exported layouts
            resourcesToRemove.push_back(layout);
    }

    QVector<QnUuid> idList;
    idList.reserve(resourcesToRemove.size());
    for (const auto& res: resourcesToRemove)
        idList.push_back(res->getId());

    qnResPool->removeResources(resourcesToRemove);
    qnResPool->removeResources(qnResPool->getAllIncompatibleResources());

    QnCameraUserAttributePool::instance()->clear();
    QnMediaServerUserAttributesPool::instance()->clear();

    propertyDictionary->clear(idList);
    qnStatusDictionary->clear(idList);

    qnLicensePool->reset();
    qnCommon->setReadOnly(false);
}

void QnWorkbenchConnectHandler::testConnectionToServer(
    const QUrl& url,
    ConnectionOptions options,
    bool force)
{
    setLogicalState(force ? LogicalState::connecting_to_target : LogicalState::testing);

    setPhysicalState(PhysicalState::testing);
    m_connecting.handle = QnAppServerConnectionFactory::ec2ConnectionFactory()->testConnection(
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
    QUrl currentUrl = QnAppServerConnectionFactory::url();
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

    m_reconnectDialog = new QnReconnectInfoDialog(mainWindow());
    connect(m_reconnectDialog, &QDialog::rejected, this,
        [this]
        {
            stopReconnecting();
            disconnectFromServer(true);
        });
    m_reconnectDialog->setServers(m_reconnectHelper->servers());
    m_reconnectDialog->setCurrentServer(m_reconnectHelper->currentServer());
    QnDialog::show(m_reconnectDialog);
    connectToServer(m_reconnectHelper->currentUrl());
    return true;
}
