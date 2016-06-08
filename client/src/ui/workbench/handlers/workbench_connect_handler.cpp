#include "workbench_connect_handler.h"

#include <QtNetwork/QHostInfo>

#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_connection_data.h>
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
#include <core/core_settings.h>
#include <core/resource/media_server_resource.h>

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

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_welcome_screen.h>

#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

#include <utils/app_server_notification_cache.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/common/app_info.h>
#include <utils/common/collection.h>
#include <utils/common/synctime.h>
#include <utils/common/system_information.h>
#include <utils/common/url.h>
#include <network/module_finder.h>
#include <network/router.h>
#include <utils/reconnect_helper.h>

#include <nx/utils/log/log.h>

namespace
{
    const int videowallReconnectTimeoutMSec = 5000;
    const int videowallCloseTimeoutMSec = 10000;

    const int maxReconnectTimeout = 10*1000;                // 10 seconds
    const int maxVideowallReconnectTimeout = 96*60*60*1000; // 4 days

    void storeCustomConnection(const QUrl &url
        , const QString &name
        , bool autoLogin)
    {
        QnConnectionDataList connections = qnSettings->customConnections();

        QUrl urlToSave(url);
        if (!autoLogin)
            urlToSave.setPassword(QString());

        QnConnectionData connectionData(name, urlToSave);
        qnSettings->setLastUsedConnection(connectionData);

        // remove previous "Last used connection"
        connections.removeOne(QnConnectionDataList::defaultLastUsedNameKey());

        QnConnectionData selected = connections.getByName(name);
        if (qnUrlEqual(selected.url, url))
        {
            connections.removeOne(selected.name);
            connections.prepend(selected);    /* Reorder. */
        }
        else
        {
            // save "Last used connection"
            QnConnectionData last(connectionData);
            last.name = QnConnectionDataList::defaultLastUsedNameKey();
            connections.prepend(last);
        }
        qnSettings->setCustomConnections(connections);
    }

    void storeSystemConnection(const QString &systemName
        , const QUrl &url
        , bool storePassword
        , bool autoLogin)
    {
        auto lastConnections = qnCoreSettings->recentUserConnections();
        // TODO: #ynikitenkov remove outdated connection data

        const auto password = (storePassword ? url.password() : QString());
        const QnUserRecentConnectionData connectionInfo =
            { systemName, url.userName(), password, storePassword };

        const auto newEnd = std::remove_if(lastConnections.begin(), lastConnections.end()
            , [connectionInfo](const QnUserRecentConnectionData &connection)
        {
            return ((connection.systemName == connectionInfo.systemName)
                && (connection.userName == connectionInfo.userName));
        });

        lastConnections.erase(newEnd, lastConnections.end());
        lastConnections.prepend(connectionInfo);

        qnCoreSettings->setRecentUserConnections(lastConnections);
        qnSettings->setAutoLogin(autoLogin);
    }
}

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject *parent /* = 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectingMessageBox(NULL),
    m_connectingHandle(0),
    m_readyForConnection(true)
{
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionClosed,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   [this] {
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
        if(!watcher->hasMismatches())
            return;

        menu()->trigger(QnActions::VersionMismatchMessageAction);
    });

    QnWorkbenchUserWatcher* userWatcher = context()->instance<QnWorkbenchUserWatcher>();
    connect(userWatcher, &QnWorkbenchUserWatcher::reconnectRequired, this, &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(userWatcher, &QnWorkbenchUserWatcher::userChanged, this, [this](const QnUserResourcePtr &user)
    {
        QnPeerRuntimeInfo localInfo = qnRuntimeInfoManager->localInfo();
        localInfo.data.userId = user ? user->getId() : QnUuid();
        qnRuntimeInfoManager->updateLocalItem(localInfo);
    });

    connect(action(QnActions::ConnectAction),              &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(QnActions::ReconnectAction),            &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(QnActions::DisconnectAction),           &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(QnActions::OpenLoginDialogAction),      &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::showLoginDialog);
    connect(action(QnActions::BeforeExitAction),           &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_beforeExitAction_triggered);

    context()->instance<QnAppServerNotificationCache>();
}

QnWorkbenchConnectHandler::~QnWorkbenchConnectHandler()
{}

ec2::AbstractECConnectionPtr QnWorkbenchConnectHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened() {
    action(QnActions::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
    action(QnActions::OpenLoginDialogAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM #Common use conditional texts?

    hideMessageBox();

    connect(qnRuntimeInfoManager,   &QnRuntimeInfoManager::runtimeInfoChanged,  this, [this](const QnPeerRuntimeInfo &info)
    {
        if (info.uuid != qnCommon->moduleGUID())
            return;

        /* We can get here during disconnect process */
        if (connection2())
            connection2()->sendRuntimeData(info.data);
    });


    connect( QnAppServerConnectionFactory::getConnection2()->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged,
        QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime) );

    //connection2()->sendRuntimeData(QnRuntimeInfoManager::instance()->localInfo().data);
    qnCommon->setLocalSystemName(connection2()->connectionInfo().systemName);
    qnCommon->setReadOnly(connection2()->connectionInfo().ecDbReadOnly);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed() {

    if( QnAppServerConnectionFactory::getConnection2() )
    {
        disconnect( QnAppServerConnectionFactory::getConnection2().get(), nullptr, this, nullptr );
        disconnect( QnAppServerConnectionFactory::getConnection2().get(), nullptr, QnSyncTime::instance(), nullptr );
    }

    disconnect(qnRuntimeInfoManager,   &QnRuntimeInfoManager::runtimeInfoChanged,  this, NULL);

    /* Don't do anything if we are closing client. */
    if (!mainWindow())
        return;

    /* If we were not disconnected intentionally try to restore connection. */
    if (connected()) {
        if (tryToRestoreConnection())
            return;
        /* Otherwise, disconnect fully. */
        disconnectFromServer(true);
        showWelcomeScreen();
    }

    clearConnection();
}

void QnWorkbenchConnectHandler::at_connectAction_triggered() {
    // ask user if he wants to save changes
    bool force = qnRuntime->isActiveXMode() || qnRuntime->isVideoWallMode();
    if (connected() && !disconnectFromServer(force))
        return;

    qnCommon->updateRunningInstanceGuid();

    QnActionParameters parameters = menu()->currentParameters(sender());
    QUrl url = parameters.argument(Qn::UrlRole, QUrl());

    const auto connectionAlias = parameters.argument(Qn::ConnectionAliasRole, QString());
    const auto storeSettings = StoreConnectionSettings::create(connectionAlias,
        parameters.argument(Qn::StorePasswordRole, false),
        parameters.argument(Qn::AutoLoginRole, false));

    if (url.isValid())
    {
        /* ActiveX plugin */
        if (qnRuntime->isActiveXMode())
        {
            if (connectToServer(url, storeSettings, true) != ec2::ErrorCode::ok)
            {
                QnGraphicsMessageBox::information(tr("Could not connect to server..."), 1000 * 60 * 60 * 24);
                menu()->trigger(QnActions::ExitAction);
            }
        }
        else if (qnRuntime->isVideoWallMode())  /* Videowall item */
        {
            //TODO: #GDM #High videowall should try indefinitely
            if (connectToServer(url, storeSettings, true) != ec2::ErrorCode::ok)
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
            if (connectToServer(url, storeSettings, false) != ec2::ErrorCode::ok)
                showWelcomeScreen();
        }
    }
    else
    {
        /* Try to load last used connection. */
        url = qnSettings->lastUsedConnection().url;
        if (!url.isValid())
            url = qnSettings->defaultConnection().url;

        /* Try to connect with saved password. */
        const bool autoLogin = qnSettings->autoLogin();
        if (autoLogin && url.isValid() && !url.password().isEmpty())
        {
            const auto storeSettings = StoreConnectionSettings::create(
                connectionAlias, true, true);

            if (connectToServer(url, storeSettings, false) != ec2::ErrorCode::ok)
                showWelcomeScreen();
        }
        else
        {
            /* No saved password, just show Welcome Screen. */
            showWelcomeScreen();
        }
    }
}

void QnWorkbenchConnectHandler::at_reconnectAction_triggered() {
    /* Reconnect call should not be executed while we are disconnected. */
    if (!context()->user())
        return;

    QUrl currentUrl = QnAppServerConnectionFactory::url();
    if (connected())
        disconnectFromServer(true);

    // Do not store connections in case of reconnection
    if (connectToServer(currentUrl, StoreConnectionSettingsPtr(), false) != ec2::ErrorCode::ok)
        showWelcomeScreen();
}

void QnWorkbenchConnectHandler::at_disconnectAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    bool force = parameters.hasArgument(Qn::ForceRole)
        ? parameters.argument(Qn::ForceRole).toBool()
        : false;
    disconnectFromServer(force);
}

bool QnWorkbenchConnectHandler::connected() const {
    return !qnCommon->remoteGUID().isNull();
}

QnWorkbenchConnectHandler::StoreConnectionSettingsPtr
QnWorkbenchConnectHandler::StoreConnectionSettings::create(
    const QString &connectionAlias,
    bool storePassword,
    bool autoLogin)
{
    const StoreConnectionSettingsPtr result(new StoreConnectionSettings());
    result->connectionAlias = connectionAlias;
    result->storePassword = storePassword;
    result->autoLogin = autoLogin;
    return result;
}

ec2::ErrorCode QnWorkbenchConnectHandler::connectToServer(const QUrl &appServerUrl
    , const StoreConnectionSettingsPtr &storeSettings
    , bool silent)
{
    if (!silent) {
        NX_ASSERT(!connected());
        if (connected())
            return ec2::ErrorCode::ok;
    }

    /* Hiding message box from previous connect. */
    hideMessageBox();

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
        if (!silent)
            m_connectingMessageBox = QnGraphicsMessageBox::information(tr("Connecting..."), INT_MAX);

        //here we are going to inner event loop
        errCode = static_cast<ec2::ErrorCode>(result.exec());
        NX_LOG(lit("Connection error %1").arg((int)errCode), cl_logDEBUG2);

        /* Hiding message box from current connect. */
        hideMessageBox();

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
    QnConnectionDiagnosticsHelper::Result status = silent
        ? QnConnectionDiagnosticsHelper::validateConnectionLight(connectionInfo, errCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errCode, appServerUrl, mainWindow());


    const auto systemName = connectionInfo.systemName;
    const auto handleConnectionResult = [systemName, appServerUrl, storeSettings] (ec2::ErrorCode code)
    {
        if (storeSettings && (code == ec2::ErrorCode::ok))
        {
            storeSystemConnection(systemName, appServerUrl,
                storeSettings->storePassword, storeSettings->autoLogin);
            storeCustomConnection(appServerUrl,
                storeSettings->connectionAlias, storeSettings->autoLogin);
        }

        return code;
    };

    switch (status) {
    case QnConnectionDiagnosticsHelper::Result::Success:
        break;
    case QnConnectionDiagnosticsHelper::Result::RestartRequested:
        menu()->trigger(QnActions::DelayedForcedExitAction);
        return handleConnectionResult(ec2::ErrorCode::ok); // to avoid cycle
    default:    //error
    {
        /* Substitute value for incompatible peers. */
        const auto code = (errCode == ec2::ErrorCode::ok
            ? ec2::ErrorCode::incompatiblePeer : errCode);

        return handleConnectionResult(code);
    }
    }

    QnAppServerConnectionFactory::setUrl(connectionInfo.ecUrl);
    QnAppServerConnectionFactory::setEc2Connection(result.connection());
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnClientMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(
        connectionInfo.effectiveUserName.isEmpty()
            ? appServerUrl.userName()
            : connectionInfo.effectiveUserName);

    //QnRouter::instance()->setEnforcedConnection(QnRoutePoint(connectionInfo.ecsGuid, connectionInfo.ecUrl.host(), connectionInfo.ecUrl.port()));

    return handleConnectionResult(ec2::ErrorCode::ok);
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force) {
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
        return false;

    if (!force)
    {
        QnGlobalSettings::instance()->synchronizeNow();
        qnSettings->setLastUsedConnection(QnConnectionData());
    }

    if (context()->user())
        hideMessageBox();

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


void QnWorkbenchConnectHandler::hideMessageBox() {
    if (!m_connectingMessageBox)
        return;

    m_connectingMessageBox->disconnect(this);
    m_connectingMessageBox->hideImmideately();
    m_connectingMessageBox = NULL;
}


void QnWorkbenchConnectHandler::showLoginDialog()
{
    // TODO: #ynikitenkov remove login dialog direct call from welcome screen
    const auto welcome = context()->instance<QnWorkbenchWelcomeScreen>();
    welcome->connectToAnotherSystem();
}

void QnWorkbenchConnectHandler::showWelcomeScreen() {
    if (qnRuntime->isActiveXMode() || qnRuntime->isVideoWallMode())
        return;

    if (context()->user())
    {
        showLoginDialog();
    }
    else
    {
        const auto welcome = context()->instance<QnWorkbenchWelcomeScreen>();
        welcome->setVisible(true);
    }

    m_connectingHandle = 0;
    hideMessageBox();
}


void QnWorkbenchConnectHandler::clearConnection()
{
    context()->instance<QnWorkbenchStateManager>()->tryClose(true);

    /* Get ready for the next connection. */
    m_readyForConnection = true;

    action(QnActions::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
    action(QnActions::OpenLoginDialogAction)->setText(tr("Connect to Server..."));

    /* Remove all remote resources. */
    QnResourceList resourcesToRemove = qnResPool->getResourcesWithFlag(Qn::remote);

    /* Also remove layouts that were just added and have no 'remote' flag set. */
    foreach (const QnLayoutResourcePtr& layout, qnResPool->getResources<QnLayoutResource>())
    {
        bool isLocal = snapshotManager()->isLocal(layout);
        bool isFile = layout->isFile();
        if (!(isLocal && isFile))  //do not remove exported layouts
            resourcesToRemove.push_back(layout);
    }

    QVector<QnUuid> idList;
    idList.reserve(resourcesToRemove.size());
    foreach(const QnResourcePtr& res, resourcesToRemove)
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

bool QnWorkbenchConnectHandler::tryToRestoreConnection() {
    QUrl currentUrl = QnAppServerConnectionFactory::url();
    if (currentUrl.isEmpty())
        return false;

    QScopedPointer<QnReconnectHelper> reconnectHelper(new QnReconnectHelper());
    if (reconnectHelper->servers().isEmpty())
        return false;

    QPointer<QnReconnectInfoDialog> reconnectInfoDialog(new QnReconnectInfoDialog(mainWindow()));
    reconnectInfoDialog->setServers(reconnectHelper->servers());

    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    reconnectInfoDialog.data(),     &QDialog::hide);

    QnDialog::show(reconnectInfoDialog);

    bool success = false;

    /* Here we will wait for the reconnect or cancel. */
    while (reconnectInfoDialog && !reconnectInfoDialog->wasCanceled()) {
        reconnectInfoDialog->setCurrentServer(reconnectHelper->currentServer());

        /* Here inner event loop will be started. */
        ec2::ErrorCode errCode = connectToServer(reconnectHelper->currentUrl()
            , StoreConnectionSettingsPtr(), true);

        /* Main window can be closed in the event loop so the dialog will be freed. */
        if (!reconnectInfoDialog)
            return true;

        /* If user press cancel while we are connecting, connection should be broken. */
        if (reconnectInfoDialog && reconnectInfoDialog->wasCanceled()) {
            reconnectInfoDialog->hide();
            reconnectInfoDialog->deleteLater();
            return false;
        }

        if (errCode == ec2::ErrorCode::ok) {
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
        do {
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


void QnWorkbenchConnectHandler::at_beforeExitAction_triggered() {
    disconnectFromServer(true);
}


