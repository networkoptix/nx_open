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
#include <utils/reconnect_helper.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>

namespace {


static const int kVideowallCloseTimeoutMSec = 10000;
static const int kMessagesDelayMs = 5000;

void storeSystemConnection(const QString &systemName, QUrl url,
    bool storePassword, bool autoLogin, bool forceRemoveOldConnection)
{
    auto recentConnections = qnClientCoreSettings->recentUserConnections();
    // TODO: #ynikitenkov remove outdated connection data

    if (autoLogin)
        storePassword = true;

    if (!storePassword)
        url.setPassword(QString());

    const auto itFoundConnection = std::find_if(recentConnections.begin(), recentConnections.end(),
        [systemName, userName = url.userName()](const QnUserRecentConnectionData& connection)
        {
            return QString::compare(connection.systemName, systemName, Qt::CaseInsensitive) == 0
                && QString::compare(connection.url.userName(), userName, Qt::CaseInsensitive) == 0;
        });

    QnUserRecentConnectionData targetConnection(QString(), systemName, url, storePassword);

    if (itFoundConnection != recentConnections.end())
    {
        if (forceRemoveOldConnection)
        {
            if (!itFoundConnection->name.isEmpty())
            {
                // If it is connection stored from Login dialog - we just clean its password
                targetConnection.name = itFoundConnection->name;
                targetConnection.isStoredPassword = false;
            }
        }
        else if (storePassword)
        {
            if (itFoundConnection->isStoredPassword || !itFoundConnection->name.isEmpty())  // if it is saved connection
                targetConnection.name = itFoundConnection->name;
        }
        else
        {
            targetConnection.name = itFoundConnection->name;
            targetConnection.isStoredPassword = itFoundConnection->isStoredPassword;
            targetConnection.url.setPassword(itFoundConnection->url.password());
        }
        recentConnections.erase(itFoundConnection);
    }
    recentConnections.prepend(targetConnection);

    qnSettings->setLastUsedConnection(targetConnection);
    qnClientCoreSettings->setRecentUserConnections(recentConnections);
    qnSettings->setAutoLogin(autoLogin);
    qnSettings->save();
}

ec2::ApiClientInfoData clientInfo()
{
    ec2::ApiClientInfoData clientData;
    clientData.id = qnSettings->pcUuid();
    clientData.fullVersion = QnAppInfo::applicationFullVersion();
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

void trace(const QString& message)
{
    qDebug() << "QnWorkbenchConnectHandler: " << message;
    //NX_LOG(lit("QnWorkbenchConnectHandler: ") + message, cl_logDEBUG1);
}

} //anonymous namespace

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectingHandle(0),
    m_state(),
    m_warnMessagesDisplayed(false)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed, this,
        &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        [this]
        {
            /* We could get here if server advanced settings were changed so peer was reset. */
            if (m_state.state() == QnConnectionState::Ready)
                return;

            NX_ASSERT(m_state.state() == QnConnectionState::Connected);
            if (m_state.state() != QnConnectionState::Connected)
            {
                disconnectFromServer(true);
                return;
            }
            trace(lit("resources received, state -> Ready"));
            m_state.setState(QnConnectionState::Ready);

            /* Reload all dialogs and dependent data. */
            context()->instance<QnWorkbenchStateManager>()->forcedUpdate();

            /* In several seconds after connect show warnings. */
            executeDelayed([this]{ showWarnMessagesOnce(); }, kMessagesDelayMs);
        });

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
    connect(action(QnActions::ReconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(QnActions::DisconnectAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(QnActions::OpenLoginDialogAction), &QAction::triggered, this,
        &QnWorkbenchConnectHandler::showLoginDialog);
    connect(action(QnActions::BeforeExitAction), &QAction::triggered, this,
        [this]
        {
            disconnectFromServer(true);
        });

    connect(action(QnActions::LogoutFromCloud), &QAction::triggered, this,
        [this]
        {
            /* Check if we need to logout if logged in under this user. */
            if (m_state.state() == QnConnectionState::Disconnected)
                return;

            QString currentLogin = QnAppServerConnectionFactory::url().userName();
            NX_ASSERT(!currentLogin.isEmpty());
            if (currentLogin.isEmpty())
                return;

            if (qnCloudStatusWatcher->effectiveUserName() == currentLogin)
                disconnectFromServer(true);
        });

    context()->instance<QnAppServerNotificationCache>();

    const auto resourceModeAction = action(QnActions::ResourcesModeAction);
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();

    connect(resourceModeAction, &QAction::toggled, this,
        [this, welcomeScreen](bool checked) { welcomeScreen->setVisible(!checked); });

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    connect(&m_state, &QnClientConnectionStatus::stateChanged, this,
        [this, welcomeScreen, resourceModeAction](QnConnectionState state)
        {
            switch (state)
            {
                case QnConnectionState::Disconnected:
                {
                    welcomeScreen->setGlobalPreloaderVisible(false);
                    resourceModeAction->setChecked(false);  //< Shows welcome screen
                    break;
                }
                case QnConnectionState::Connecting:
                case QnConnectionState::Connected:
                    welcomeScreen->setGlobalPreloaderVisible(true);
                    break;
                case QnConnectionState::Ready:
                    welcomeScreen->setGlobalPreloaderVisible(false);
                    resourceModeAction->setChecked(true); //< Hides welcome screen
                    break;
                default:
                    break;
            }
        });
}

QnWorkbenchConnectHandler::~QnWorkbenchConnectHandler()
{
}

void QnWorkbenchConnectHandler::handleConnectReply(
    int handle,
    ec2::ErrorCode errorCode,
    ec2::AbstractECConnectionPtr connection,
    const ConnectionSettingsPtr &storeSettings)
{
    /* Check if we have entered 'connect' method again while were in 'connecting...' state */
    if (m_connectingHandle != handle)
        return;

    /* We've got another connectionOpened() because client was on a breakpoint. */
    if (m_state == QnConnectionState::Connected)
        return;

    auto validState = m_state.state() == QnConnectionState::Connecting
        || m_state.state() == QnConnectionState::Reconnecting;
    NX_ASSERT(validState);
    if (!validState)
        return;
    m_connectingHandle = 0;

    /* Preliminary exit if application was closed while we were in the inner loop. */
    NX_ASSERT(!context()->closingDown());
    if (context()->closingDown())
        return;

    NX_ASSERT(connection || errorCode != ec2::ErrorCode::ok);
    QnConnectionInfo connectionInfo;
    if (connection)
        connectionInfo = connection->connectionInfo();

    const bool silent = m_state.state() == QnConnectionState::Reconnecting
        || !qnRuntime->isDesktopMode();

    auto status = silent
        ? QnConnectionValidator::validateConnection(connectionInfo, errorCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errorCode, mainWindow());
    NX_ASSERT(connection || status != Qn::ConnectionResult::Success);

    if (m_state.state() == QnConnectionState::Reconnecting)
    {
        processReconnectingReply(status, connection);
        return;
    }

    switch (status)
    {
        case Qn::ConnectionResult::Success:
            storeConnectionRecord(connectionInfo, storeSettings);
            if (connectionInfo.newSystem)
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
        case Qn::ConnectionResult::IncompatibleProtocol:
            storeConnectionRecord(connectionInfo, storeSettings);
            menu()->trigger(QnActions::DelayedForcedExitAction);
            break;
        default:    //error
            if (!qnRuntime->isDesktopMode())
            {
                QnGraphicsMessageBox* incompatibleMessageBox =
                    QnGraphicsMessageBox::informationTicking(
                        tr("Could not connect to server. Closing in %1..."),
                        kVideowallCloseTimeoutMSec);
                connect(incompatibleMessageBox, &QnGraphicsMessageBox::finished,
                    action(QnActions::ExitAction), &QAction::trigger);
            }
            else
            {
                disconnectFromServer(true);
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
    bool success = status == Qn::ConnectionResult::Success;
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

    //TODO: #ak fix server behavior
    /* When client tries to reconnect to a single server very fast we can
    * get "unauthorized" reply, because we try to connect before the
    * server have initialized its auth classes. Need to return
    * temporaryUnauthorized error code in this case.
    */
    switch (status)
    {
        case Qn::ConnectionResult::Unauthorized:
//          m_reconnectHelper->markServerAsInvalid(m_reconnectHelper->currentServer());
            break;
        case Qn::ConnectionResult::IncompatibleInternal:
        case Qn::ConnectionResult::IncompatibleVersion:
        case Qn::ConnectionResult::IncompatibleProtocol:
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
        connectToServer(m_reconnectHelper->currentUrl(), ConnectionSettingsPtr());
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

    QUrl url = connectionInfo.effectiveUrl();
    QnAppServerConnectionFactory::setUrl(url);
    QnAppServerConnectionFactory::setEc2Connection(connection);
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);
    QnClientMessageProcessor::instance()->init(connection);

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
        ? url.userName()
        : connectionInfo.effectiveUserName);
}

void QnWorkbenchConnectHandler::storeConnectionRecord(
    const QnConnectionInfo& info,
    const ConnectionSettingsPtr& storeSettings)
{
    if (!storeSettings)
        return;

    storeSystemConnection(
        info.systemName,
        info.ecUrl,
        storeSettings->storePassword,
        storeSettings->autoLogin,
        storeSettings->forceRemoveOldConnection);
}

void QnWorkbenchConnectHandler::showWarnMessagesOnce()
{
    if (m_state.state() != QnConnectionState::Ready)
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
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened()
{
    trace(lit("connection opened, state -> Connected"));
    m_state.setState(QnConnectionState::Connected);
    action(QnActions::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
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

    qnCommon->setLocalSystemName(connection->connectionInfo().systemName);
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

    /* If we were not disconnected intentionally try to restore connection. */
    if (m_state.state() == QnConnectionState::Ready)
    {
        if (tryToRestoreConnection())
            return;
    }
    disconnectFromServer(true);
}

void QnWorkbenchConnectHandler::at_connectAction_triggered()
{
    bool force = qnRuntime->isActiveXMode() || qnRuntime->isVideoWallMode();
    if (m_state.state() == QnConnectionState::Ready)
    {
        // ask user if he wants to save changes
        if (!disconnectFromServer(force))
            return;
    }
    else
    {
        // break 'Connecting' state if any
        disconnectFromServer(true);
    }

    qnCommon->updateRunningInstanceGuid();

    QnActionParameters parameters = menu()->currentParameters(sender());
    QUrl url = parameters.argument(Qn::UrlRole, QUrl());

    const auto settings = ConnectionSettings::create(
        parameters.argument(Qn::StorePasswordRole, false),
        parameters.argument(Qn::AutoLoginRole, false),
        parameters.argument(Qn::ForceRemoveOldConnectionRole, false),
        parameters.argument(Qn::CompletionWatcherRole, QnRaiiGuardPtr()));

    if (url.isValid())
    {
        trace(lit("state -> Connecting"));
        m_state.setState(QnConnectionState::Connecting);
        connectToServer(url, settings);
    }
    else
    {
        /* Try to load last used connection. */
        url = qnSettings->lastUsedConnection().url;

        /* Try to connect with saved password. */
        const bool autoLogin = qnSettings->autoLogin();
        if (autoLogin && url.isValid() && !url.password().isEmpty())
        {
            const auto connectionSettings = ConnectionSettings::create(
                false, true, false, settings->completionWatcher);

            trace(lit("state -> Connecting"));
            m_state.setState(QnConnectionState::Connecting);
            connectToServer(url, connectionSettings);
        }
    }
}

void QnWorkbenchConnectHandler::at_reconnectAction_triggered()
{
    /* Reconnect call should not be executed while we are disconnected. */
    if (m_state.state() != QnConnectionState::Ready)
        return;

    QUrl currentUrl = QnAppServerConnectionFactory::url();
    disconnectFromServer(true);

    // Do not store connections in case of reconnection
    trace(lit("state -> Connecting"));
    m_state.setState(QnConnectionState::Connecting);
    connectToServer(currentUrl, ConnectionSettingsPtr());
}

void QnWorkbenchConnectHandler::at_disconnectAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    bool force = parameters.hasArgument(Qn::ForceRole)
        ? parameters.argument(Qn::ForceRole).toBool()
        : false;
    disconnectFromServer(force);
    qnSettings->setAutoLogin(false);
    qnSettings->save();
}

QnWorkbenchConnectHandler::ConnectionSettingsPtr
QnWorkbenchConnectHandler::ConnectionSettings::create(
    bool storePassword,
    bool autoLogin,
    bool forceRemoveOldConnection,
    const QnRaiiGuardPtr& completionWatcher)
{
    const ConnectionSettingsPtr result(new ConnectionSettings());
    result->storePassword = storePassword;
    result->autoLogin = autoLogin;
    result->forceRemoveOldConnection = forceRemoveOldConnection;
    result->completionWatcher = completionWatcher;
    return result;
}

void QnWorkbenchConnectHandler::connectToServer(const QUrl &url,
    const ConnectionSettingsPtr &storeSettings)
{
    auto validState = m_state.state() == QnConnectionState::Connecting
        || m_state.state() == QnConnectionState::Reconnecting;
    NX_ASSERT(validState);
    if (!validState)
        return;

    m_connectingHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
        url, clientInfo(), this,
        [this, storeSettings]
        (int handle, ec2::ErrorCode errorCode, ec2::AbstractECConnectionPtr connection)
        {
            handleConnectReply(handle, errorCode, connection, storeSettings);
        });
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force)
{
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
    {
        NX_ASSERT(!force, "Forced exit must close connection");
        return false;
    }

    if (!force)
    {
        QnGlobalSettings::instance()->synchronizeNow();
        qnSettings->setLastUsedConnection(QnUserRecentConnectionData());
    }

    clearConnection();
    showWelcomeScreen();
    return true;
}

void QnWorkbenchConnectHandler::showLoginDialog()
{
    if (context()->closingDown())
        return;

    const QScopedPointer<QnLoginDialog> dialog(new QnLoginDialog(context()->mainWindow()));
    dialog->exec();
}

void QnWorkbenchConnectHandler::showWelcomeScreen()
{
    if (!qnRuntime->isDesktopMode())
        return;

    if (context()->closingDown())
        return;

    if (m_state.state() != QnConnectionState::Disconnected)
    {
        showLoginDialog();
    }
    else
    {
        auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
        welcomeScreen->setGlobalPreloaderVisible(false);
        action(QnActions::ResourcesModeAction)->setChecked(false); //< Show welcome screen
    }
}

void QnWorkbenchConnectHandler::clearConnection()
{
    trace(lit("state -> Disconnected"));
    m_state.setState(QnConnectionState::Disconnected);
    m_connectingHandle = 0;
    stopReconnecting();

    QnClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());
    QnSessionManager::instance()->stop();
    QnResource::stopCommandProc();

    context()->setUserName(QString());

    /* Get ready for the next connection. */
    m_warnMessagesDisplayed = false;

    action(QnActions::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
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
    qnCommon->setLocalSystemName(QString());
    qnCommon->setReadOnly(false);
}

bool QnWorkbenchConnectHandler::tryToRestoreConnection()
{
    QUrl currentUrl = QnAppServerConnectionFactory::url();
    NX_ASSERT(!currentUrl.isEmpty());
    if (currentUrl.isEmpty())
        return false;

    NX_ASSERT(!m_reconnectHelper);
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
    trace(lit("state -> Reconnecting"));
    m_state.setState(QnConnectionState::Reconnecting);
    connectToServer(m_reconnectHelper->currentUrl(), ConnectionSettingsPtr());
    return true;
}
