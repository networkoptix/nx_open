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
#include <network/module_finder.h>
#include <network/router.h>
#include <utils/reconnect_helper.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/log/log.h>

namespace {

const int videowallReconnectTimeoutMSec = 5000;
const int videowallCloseTimeoutMSec = 10000;

const int maxReconnectTimeout = 10 * 1000;                      // 10 seconds
const int maxVideowallReconnectTimeout = 96 * 60 * 60 * 1000;   // 4 days

void storeSystemConnection(const QString &systemName, QUrl url,
    bool storePassword, bool autoLogin, bool forceRemoveOldConnection)
{
    auto recentConnections = qnClientCoreSettings->recentUserConnections();
    // TODO: #ynikitenkov remove outdated connection data

    if (autoLogin)
        storePassword = true;

    if (!storePassword)
        url.setPassword(QString());

    const auto itFoundConnection = std::find_if(recentConnections.begin(), recentConnections.end()
        , [systemName, userName = url.userName()](const QnUserRecentConnectionData& connection)
    {
        return ((connection.systemName == systemName) &&
            (connection.url.userName() == userName));
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

} //anonymous namespace

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_processingConnectAction(false),
    m_connectingHandle(0),
    m_readyForConnection(true)
{
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened, this, &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionClosed, this, &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived, this,
        [this]
        {
            /* Reload all dialogs and dependent data. */
            context()->instance<QnWorkbenchStateManager>()->forcedUpdate();

            menu()->triggerIfPossible(QnActions::AllowStatisticsReportMessageAction);

            /* Collect and send crash dumps if allowed */
            m_crashReporter.scanAndReportAsync(qnSettings->rawSettings());

            /* We are just reconnected automatically, e.g. after update. */
            if (!m_readyForConnection)
                return;

            m_readyForConnection = false;

            QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
            if (!watcher->hasMismatches())
                return;

            menu()->trigger(QnActions::VersionMismatchMessageAction);
        });

    QnWorkbenchUserWatcher* userWatcher = context()->instance<QnWorkbenchUserWatcher>();
    connect(userWatcher, &QnWorkbenchUserWatcher::reconnectRequired, this, &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(userWatcher, &QnWorkbenchUserWatcher::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            QnPeerRuntimeInfo localInfo = qnRuntimeInfoManager->localInfo();
            localInfo.data.userId = user ? user->getId() : QnUuid();
            qnRuntimeInfoManager->updateLocalItem(localInfo);
        });

    connect(action(QnActions::ConnectAction),       &QAction::triggered, this, &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(QnActions::ReconnectAction),     &QAction::triggered, this, &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(QnActions::DisconnectAction),    &QAction::triggered, this, &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(QnActions::OpenLoginDialogAction), &QAction::triggered, this, &QnWorkbenchConnectHandler::showLoginDialog);
    connect(action(QnActions::BeforeExitAction),    &QAction::triggered, this, &QnWorkbenchConnectHandler::at_beforeExitAction_triggered);

    context()->instance<QnAppServerNotificationCache>();


    const auto resourceModeAction = action(QnActions::ResourcesModeAction);
    const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();

    connect(resourceModeAction, &QAction::toggled, this,
        [this, welcomeScreen](bool checked) { welcomeScreen->setVisible(!checked); });

    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        [resourceModeAction]() { resourceModeAction->setChecked(true); });

    connect(qnDesktopClientMessageProcessor, &QnDesktopClientMessageProcessor::connectionStateChanged, this,
        [this, welcomeScreen, resourceModeAction]()
    {
        const auto state = qnDesktopClientMessageProcessor->connectionState();
        switch (state)
        {
            case QnConnectionState::Disconnected:
            {
                if (!m_processingConnectAction)
                {
                    welcomeScreen->setGlobalPreloaderVisible(false);
                    resourceModeAction->setChecked(false);  //< Shows welcome screen
                }
                break;
            }
            case QnConnectionState::Connecting:
            case QnConnectionState::Connected:
                welcomeScreen->setGlobalPreloaderVisible(true);
                resourceModeAction->setChecked(false);  //< Shows welcome screen
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

ec2::AbstractECConnectionPtr QnWorkbenchConnectHandler::connection2() const
{
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened()
{
    action(QnActions::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
    action(QnActions::OpenLoginDialogAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM #Common use conditional texts?

    connect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, this, [this](const QnPeerRuntimeInfo &info)
    {
        if (info.uuid != qnCommon->moduleGUID())
            return;

        /* We can get here during disconnect process */
        if (connection2())
            connection2()->sendRuntimeData(info.data);
    });


    connect(connection2()->getTimeNotificationManager().get(), &ec2::AbstractTimeNotificationManager::timeChanged,
        QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime));

    //connection2()->sendRuntimeData(QnRuntimeInfoManager::instance()->localInfo().data);
    qnCommon->setLocalSystemName(connection2()->connectionInfo().systemName);
    qnCommon->setReadOnly(connection2()->connectionInfo().ecDbReadOnly);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed()
{

    if (connection2())
    {
        disconnect(connection2().get(), nullptr, this, nullptr);
        disconnect(connection2().get(), nullptr, QnSyncTime::instance(), nullptr);
    }

    disconnect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoChanged, this, NULL);

    /* Don't do anything if we are closing client. */
    if (!mainWindow())
        return;

    /* If we were not disconnected intentionally try to restore connection. */
    if (connected())
    {
        if (tryToRestoreConnection())
            return;
        /* Otherwise, disconnect fully. */
        disconnectFromServer(true);
        showWelcomeScreen();
    }

    clearConnection();
}

void QnWorkbenchConnectHandler::at_connectAction_triggered()
{
    if (m_processingConnectAction)
        return;

    m_processingConnectAction = true;
    auto uncheckConnectActionRaii = QnRaiiGuard::createDestructable(
        [this]() { m_processingConnectAction = false; });

    // ask user if he wants to save changes
    bool force = qnRuntime->isActiveXMode() || qnRuntime->isVideoWallMode();
    if (connected() && !disconnectFromServer(force))
        return;

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
        /* ActiveX plugin */
        if (qnRuntime->isActiveXMode())
        {
            if (connectToServer(url, settings, true) != ec2::ErrorCode::ok)
            {
                QnGraphicsMessageBox::information(tr("Could not connect to server..."), 1000 * 60 * 60 * 24);
                menu()->trigger(QnActions::ExitAction);
            }
        }
        else if (qnRuntime->isVideoWallMode())  /* Videowall item */
        {
            //TODO: #GDM #High videowall should try indefinitely
            if (connectToServer(url, settings, true) != ec2::ErrorCode::ok)
            {
                QnGraphicsMessageBox* incompatibleMessageBox =
                    QnGraphicsMessageBox::informationTicking(tr("Could not connect to server. Closing in %1...")
                        , videowallCloseTimeoutMSec);
                connect(incompatibleMessageBox, &QnGraphicsMessageBox::finished
                    , action(QnActions::ExitAction), &QAction::trigger);
            }
        }
        else
        {
            /* Login Dialog or 'Open in new window' with url */

            //try connect; if not - show login dialog
            if (connectToServer(url, settings, false) != ec2::ErrorCode::ok)
                showWelcomeScreen();
        }
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

            if (connectToServer(url, connectionSettings, false) != ec2::ErrorCode::ok)
                showWelcomeScreen();
        }
        else
        {
            /* No saved password, just show Welcome Screen. */
            showWelcomeScreen();
        }
    }
}

void QnWorkbenchConnectHandler::at_reconnectAction_triggered()
{
    /* Reconnect call should not be executed while we are disconnected. */
    if (!context()->user())
        return;

    QUrl currentUrl = QnAppServerConnectionFactory::url();
    if (connected())
        disconnectFromServer(true);

    // Do not store connections in case of reconnection
    if (connectToServer(currentUrl, ConnectionSettingsPtr(), false) != ec2::ErrorCode::ok)
        showWelcomeScreen();
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

bool QnWorkbenchConnectHandler::connected() const
{
    return !qnCommon->remoteGUID().isNull();
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

ec2::ErrorCode QnWorkbenchConnectHandler::connectToServer(const QUrl &appServerUrl,
    const ConnectionSettingsPtr &storeSettings,
    bool silent)
{
    if (!silent)
    {
        NX_ASSERT(!connected());
        if (connected())
            return ec2::ErrorCode::ok;
    }

    ec2::ApiClientInfoData clientData;
    {
        clientData.id = qnSettings->pcUuid();
        clientData.skin = lit("Dark");
        clientData.fullVersion = QnAppInfo::applicationFullVersion();
        clientData.systemInfo = QnSystemInformation::currentSystemInformation().toString();
        clientData.systemRuntime = QnSystemInformation::currentSystemRuntime();

        const auto& hw = HardwareInformation::instance();
        clientData.phisicalMemory = hw.phisicalMemory;
        clientData.cpuArchitecture = hw.cpuArchitecture;
        clientData.cpuModelName = hw.cpuModelName;

        const auto gl = QnGlFunctions::openGLCachedInfo();
        clientData.openGLRenderer = gl.renderer;
        clientData.openGLVersion = gl.version;
        clientData.openGLVendor = gl.vendor;
    }

    QnEc2ConnectionRequestResult result;
    ec2::ErrorCode errCode = ec2::ErrorCode::ok;

    {
        m_connectingHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
            appServerUrl, clientData, &result, &QnEc2ConnectionRequestResult::processEc2Reply);

        //here we are going to inner event loop
        errCode = static_cast<ec2::ErrorCode>(result.exec());
        NX_LOG(lit("Connection error %1").arg((int)errCode), cl_logDEBUG2);

        /* Check if we have entered 'connect' method again while were in 'connecting...' state */
        if (m_connectingHandle != result.handle())
            return ec2::ErrorCode::ok;  // We don't need to use handleConnectionResult here
                                        // because it is just exit from subsequent connect call
    }
    m_connectingHandle = 0;

    /* Preliminary exit if application was closed while we were in the inner loop. */
    if (context()->closingDown())
        return ec2::ErrorCode::ok;

    const QnConnectionInfo connectionInfo = result.reply<QnConnectionInfo>();
    // TODO: check me!
    auto status = silent
        ? QnConnectionValidator::validateConnection(connectionInfo, errCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errCode, appServerUrl, mainWindow());

    const auto systemName = connectionInfo.systemName;
    const auto storeConnection = [systemName, appServerUrl, storeSettings]()
    {
        if (storeSettings)
        {
            storeSystemConnection(systemName, appServerUrl, storeSettings->storePassword,
                storeSettings->autoLogin, storeSettings->forceRemoveOldConnection);
        }
    };

    switch (status)
    {
        case Qn::ConnectionResult::success:
            break;
        case Qn::ConnectionResult::compatibilityMode:
            storeConnection();
            menu()->trigger(QnActions::DelayedForcedExitAction);
            return ec2::ErrorCode::ok; // to avoid cycle
        default:    //error
        {
            /* Substitute value for incompatible peers. */
            return errCode == ec2::ErrorCode::ok
                ? ec2::ErrorCode::incompatiblePeer
                : errCode;
        }
    }

    if (connectionInfo.newSystem)
    {
        storeConnection();
        showWelcomeScreen();
        auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();
        welcomeScreen->setupFactorySystem(connectionInfo.ecUrl.toString());
        return ec2::ErrorCode::ok;
    }

    QUrl ecUrl = connectionInfo.ecUrl;
    if (connectionInfo.allowSslConnections)
        ecUrl.setScheme(lit("https"));

    QnAppServerConnectionFactory::setUrl(ecUrl);
    QnAppServerConnectionFactory::setEc2Connection(result.connection());
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnClientMessageProcessor::instance()->init(connection2());

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
        ? appServerUrl.userName()
        : connectionInfo.effectiveUserName);

    storeConnection();
    return ec2::ErrorCode::ok;
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force)
{
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
        return false;

    if (!force)
    {
        QnGlobalSettings::instance()->synchronizeNow();
        qnSettings->setLastUsedConnection(QnUserRecentConnectionData());
    }

    //QnRouter::instance()->setEnforcedConnection(QnRoutePoint());
    QnClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());
    QnSessionManager::instance()->stop();
    QnResource::stopCommandProc();

    context()->setUserName(QString());

    return true;
}

void QnWorkbenchConnectHandler::showLoginDialog()
{
    const QScopedPointer<QnLoginDialog> dialog(new QnLoginDialog(context()->mainWindow()));
    dialog->exec();
}

void QnWorkbenchConnectHandler::showWelcomeScreen()
{
    if (!qnRuntime->isDesktopMode())
        return;

    if (context()->user())
        showLoginDialog();
    else
        action(QnActions::ResourcesModeAction)->setChecked(false);

    m_connectingHandle = 0;
}

void QnWorkbenchConnectHandler::clearConnection()
{
    context()->instance<QnWorkbenchStateManager>()->tryClose(true);

    /* Get ready for the next connection. */
    m_readyForConnection = true;

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
    if (currentUrl.isEmpty())
        return false;

    QScopedPointer<QnReconnectHelper> reconnectHelper(new QnReconnectHelper());
    if (reconnectHelper->servers().isEmpty())
        return false;

    QPointer<QnReconnectInfoDialog> reconnectInfoDialog(new QnReconnectInfoDialog(mainWindow()));
    reconnectInfoDialog->setServers(reconnectHelper->servers());

    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened, reconnectInfoDialog.data(), &QDialog::hide);

    QnDialog::show(reconnectInfoDialog);

    bool success = false;

    /* Here we will wait for the reconnect or cancel. */
    while (reconnectInfoDialog && !reconnectInfoDialog->wasCanceled())
    {
        reconnectInfoDialog->setCurrentServer(reconnectHelper->currentServer());

        /* Here inner event loop will be started. */
        ec2::ErrorCode errCode = connectToServer(reconnectHelper->currentUrl()
            , ConnectionSettingsPtr(), true);

        /* Main window can be closed in the event loop so the dialog will be freed. */
        if (!reconnectInfoDialog)
            return true;

        /* If user press cancel while we are connecting, connection should be broken. */
        if (reconnectInfoDialog && reconnectInfoDialog->wasCanceled())
        {
            reconnectInfoDialog->hide();
            reconnectInfoDialog->deleteLater();
            return false;
        }

        if (errCode == ec2::ErrorCode::ok)
        {
            success = true;
            break;
        }

        //TODO: #dklychkov #GDM When client tries to reconnect to a single server very often we can get "unauthorized" reply,
        //      because we try to connect before the server initialized its auth classes.

        if (errCode == ec2::ErrorCode::incompatiblePeer || errCode == ec2::ErrorCode::unauthorized)
            reconnectHelper->markServerAsInvalid(reconnectHelper->currentServer());

        /* Find next valid server for reconnect. */
        QnMediaServerResourceList allServers = reconnectHelper->servers();
        bool found = true;
        do
        {
            reconnectHelper->next();
            /* We have found at least one correct interface for the server. */
            found = reconnectHelper->currentUrl().isValid();
            if (!found) /* Do not try to connect to invalid servers. */
                allServers.removeOne(reconnectHelper->currentServer());
        } while (!found && !allServers.isEmpty());

        /* Break cycle if we cannot find any valid server. */
        if (!found)
            break;
    }

    /* Main window can be closed in the event loop so the dialog will be freed. */
    if (!reconnectInfoDialog)
        return true;

    disconnect(QnClientMessageProcessor::instance(), NULL, reconnectInfoDialog, NULL);
    reconnectInfoDialog->deleteLater();
    if (reconnectInfoDialog->wasCanceled())
        return false;
    return success;
}


void QnWorkbenchConnectHandler::at_beforeExitAction_triggered()
{
    disconnectFromServer(true);
}


