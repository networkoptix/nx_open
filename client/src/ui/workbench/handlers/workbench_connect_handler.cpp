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

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>

#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/reconnect_info_dialog.h>
#include <ui/dialogs/non_modal_dialog_constructor.h>

#include <ui/graphics/items/generic/graphics_message_box.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_state_manager.h>

#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>

#include <utils/app_server_notification_cache.h>
#include <utils/connection_diagnostics_helper.h>
#include <utils/common/synctime.h>
#include <utils/network/module_finder.h>
#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>


#include "compatibility.h"

namespace {
    const int videowallReconnectTimeoutMSec = 5000;
    const int videowallCloseTimeoutMSec = 10000;

    const int maxReconnectTimeout = 10*1000;                // 10 seconds
    const int maxVideowallReconnectTimeout = 96*60*60*1000; // 4 days
}

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject *parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectingMessageBox(NULL),
    m_connectingHandle(0),
    m_readyForConnection(true)
{
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionClosed,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   [this] {
        /* We are just reconnected automatically, e.g. after update. */
        if (!m_readyForConnection)
            return;

        m_readyForConnection = false;

        QnWorkbenchVersionMismatchWatcher *watcher = context()->instance<QnWorkbenchVersionMismatchWatcher>();
        if(!watcher->hasMismatches())
            return;

        menu()->trigger(Qn::VersionMismatchMessageAction);
    });

    connect(action(Qn::ConnectAction),              &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(Qn::ReconnectAction),            &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(Qn::DisconnectAction),           &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(Qn::OpenLoginDialogAction),      &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::showLoginDialog);
    connect(action(Qn::BeforeExitAction),           &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_beforeExitAction_triggered);

    context()->instance<QnAppServerNotificationCache>();
}

QnWorkbenchConnectHandler::~QnWorkbenchConnectHandler() {
    if (loginDialog())
        delete loginDialog();
}

ec2::AbstractECConnectionPtr QnWorkbenchConnectHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

QnLoginDialog * QnWorkbenchConnectHandler::loginDialog() const {
    return m_loginDialog.data();
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened() {
    action(Qn::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/connected.png"));
    action(Qn::OpenLoginDialogAction)->setText(tr("Connect to Another Server...")); // TODO: #GDM #Common use conditional texts?

    context()->instance<QnAppServerNotificationCache>()->getFileList();

    hideMessageBox();

    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoChanged,  this, [this](const QnPeerRuntimeInfo &info) 
    {
        if (info.uuid != qnCommon->moduleGUID())
            return;
        connection2()->sendRuntimeData(info.data);
    });


    connect( QnAppServerConnectionFactory::getConnection2()->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged,
        QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime) );

    //connection2()->sendRuntimeData(QnRuntimeInfoManager::instance()->localInfo().data);
    qnCommon->setLocalSystemName(connection2()->connectionInfo().systemName);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed() {

    if( QnAppServerConnectionFactory::getConnection2() )
    {
        disconnect( QnAppServerConnectionFactory::getConnection2().get(), nullptr, this, nullptr );
        disconnect( QnAppServerConnectionFactory::getConnection2().get(), nullptr, QnSyncTime::instance(), nullptr );
    }

    disconnect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoChanged,  this, NULL);

    /* Don't do anything if we are closing client. */
    if (!mainWindow())
        return;

    /* If we were not disconnected intentionally try to restore connection. */
    if (connected()) {
        if (tryToRestoreConnection())
            return;
        /* Otherwise, disconnect fully. */    
        disconnectFromServer(true);
        showLoginDialog();
    }
        
    clearConnection();
}

void QnWorkbenchConnectHandler::at_connectAction_triggered() {
    // ask user if he wants to save changes
    if (connected() && !disconnectFromServer(false))
        return; 

    QnActionParameters parameters = menu()->currentParameters(sender());
    QUrl url = parameters.argument(Qn::UrlRole, QUrl());

    if (url.isValid()) {
        /* Videowall item */
        if (qnSettings->isVideoWallMode()) {
            if (!connectToServer(url, true)) {
                QnGraphicsMessageBox* incompatibleMessageBox = QnGraphicsMessageBox::informationTicking(tr("Could not connect to server. Closing in %1..."), videowallCloseTimeoutMSec);
                connect(incompatibleMessageBox, &QnGraphicsMessageBox::finished, action(Qn::ExitAction), &QAction::trigger);
            }
        } 
        else
        /* Login Dialog or 'Open in new window' with url */
        { 
            //try connect; if not - show login dialog
            if (!connectToServer(url))
                showLoginDialog();
        }
    } else {
        /* Try to load last used connection. */
        url = qnSettings->lastUsedConnection().url;
        if (!url.isValid())
            url = qnSettings->defaultConnection().url;

        /* Try to connect with saved password. */
        if (qnSettings->autoLogin() 
            && url.isValid() 
            && !url.password().isEmpty()) 
        {
            if (!connectToServer(url))
                showLoginDialog();
        } else 
        /* No saved password, just open Login Dialog. */ 
        {
            showLoginDialog();
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
    if (!connectToServer(currentUrl))
        showLoginDialog();
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

bool QnWorkbenchConnectHandler::connectToServer(const QUrl &appServerUrl, bool silent) {
    if (!silent) {
        Q_ASSERT(!connected());
        if (connected())
            return true;
    }
    
    /* Hiding message box from previous connect. */
    hideMessageBox();

    QnEc2ConnectionRequestResult result;
    m_connectingHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(appServerUrl, &result, &QnEc2ConnectionRequestResult::processEc2Reply );
    if (!silent)
        m_connectingMessageBox = QnGraphicsMessageBox::information(tr("Connecting..."), INT_MAX);

    //here we are going to inner event loop
    ec2::ErrorCode errCode = static_cast<ec2::ErrorCode>(result.exec());

    /* Check if we have entered 'connect' method again while were in 'connecting...' state */
    if (m_connectingHandle != result.handle())
        return true;
    m_connectingHandle = 0;

    /* Hiding message box from current connect. */
    hideMessageBox();

    QnConnectionInfo connectionInfo = result.reply<QnConnectionInfo>();
    QnConnectionDiagnosticsHelper::Result status = silent
        ? QnConnectionDiagnosticsHelper::validateConnectionLight(connectionInfo, errCode)
        : QnConnectionDiagnosticsHelper::validateConnection(connectionInfo, errCode, appServerUrl, mainWindow());
    
    switch (status) {
    case QnConnectionDiagnosticsHelper::Result::Failure:
        return false;
    case QnConnectionDiagnosticsHelper::Result::Restart:
        menu()->trigger(Qn::ExitActionDelayed);
        return true; // to avoid cycle
    default:    //success
        break;
    }

    QnAppServerConnectionFactory::setUrl(appServerUrl);
    QnAppServerConnectionFactory::setEc2Connection(result.connection());
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnClientMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(appServerUrl.userName());

    QnGlobalModuleFinder::instance()->setConnection(result.connection());

    return true;
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force) {
    if (!context()->instance<QnWorkbenchStateManager>()->tryClose(force))
        return false;

    if (!force) {
        QnGlobalSettings::instance()->synchronizeNow();
        qnSettings->setLastUsedConnection(QnConnectionData());
    }

    hideMessageBox();

    QnGlobalModuleFinder::instance()->setConnection(NULL);
    QnClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
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


void QnWorkbenchConnectHandler::showLoginDialog() {
    QnNonModalDialogConstructor<QnLoginDialog> dialogConstructor(m_loginDialog, mainWindow());
    dialogConstructor.resetGeometry();

    /* Abort current connect. */
    m_connectingHandle = 0;
    hideMessageBox();
}


void QnWorkbenchConnectHandler::clearConnection() {
    context()->instance<QnWorkbenchStateManager>()->tryClose(true);

    /* Get ready for the next connection. */
    m_readyForConnection = true;

    action(Qn::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
    action(Qn::OpenLoginDialogAction)->setText(tr("Connect to Server..."));

    /* Remove all remote resources. */
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(Qn::remote);

    QVector<QnUuid> idList;
    idList.reserve(remoteResources.size());
    foreach(const QnResourcePtr& res, remoteResources)
        idList.push_back(res->getId());

    resourcePool()->removeResources(remoteResources);
    resourcePool()->removeResources(resourcePool()->getAllIncompatibleResources());

    QnCameraUserAttributePool::instance()->clear();
    QnMediaServerUserAttributesPool::instance()->clear();

    propertyDictionary->clear(idList);
    qnStatusDictionary->clear(idList);

    /* Also remove layouts that were just added and have no 'remote' flag set. */
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources<QnLayoutResource>()) {
        bool isLocal = snapshotManager()->isLocal(layout);
        bool isFile = snapshotManager()->isFile(layout);
        if(isLocal && isFile)  //do not remove exported layouts
            continue;
        resourcePool()->removeResource(layout);
    }

    qnLicensePool->reset();
    context()->instance<QnAppServerNotificationCache>()->clear();
    qnCommon->setLocalSystemName(QString());
}

bool QnWorkbenchConnectHandler::tryToRestoreConnection() {
    QUrl currentUrl = QnAppServerConnectionFactory::url();
    if (currentUrl.isEmpty())
        return false;

    QnReconnectInfoDialog* reconnectInfoDialog = new QnReconnectInfoDialog(mainWindow());
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    reconnectInfoDialog,   &QDialog::hide);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    reconnectInfoDialog,   &QObject::deleteLater);

    reconnectInfoDialog->setText(tr("Resolving servers..."));
    reconnectInfoDialog->show();

    QString userName = currentUrl.userName();
    QString password = currentUrl.password();

    QHostAddress currentAddr(currentUrl.host());
    quint32 currentIp = currentAddr.toIPv4Address();
    if (currentAddr.isNull()) {
        QHostInfo currentInfo = QHostInfo::fromName(currentUrl.host());
        foreach (const QHostAddress &addr, currentInfo.addresses()) {
            currentIp = addr.toIPv4Address();
            if (currentIp > 0)
                break;
        }
    }

    auto matchIpMetric = [](quint32 ip1, quint32 ip2) {
        int i = 31;
        while (i >= 0 && (ip1 >> i == ip2 >> i))
            --i;
        return 31 - i;
    };

    auto hostLessThan = [currentIp, matchIpMetric](const QUrl &left, const QUrl &right) {
        QHostAddress laddr(left.host());
        QHostAddress raddr(right.host());
        if (laddr.isNull() || raddr.isNull())
            return raddr.isNull();

        int metricDiff = matchIpMetric(currentIp, laddr.toIPv4Address()) - matchIpMetric(currentIp, raddr.toIPv4Address());
        return metricDiff != 0
            ? metricDiff > 0
            : laddr.toIPv4Address() < raddr.toIPv4Address();
    };

    QList<QnUuid> allServers;
    foreach (const QnMediaServerResourcePtr &server, qnResPool->getResources<QnMediaServerResource>())
        allServers << server->getId();

    QList<QUrl> availableServers;
    QMap<QString, int> hostsCount; // count duplicated hosts to display port in their url

    foreach (const QnModuleInformation &info, QnModuleFinder::instance()->foundModules()) {
        if (!allServers.contains(info.id))
            continue;

        foreach (const QString &remoteAddr, info.remoteAddresses) {
            QUrl serverUrl;
            serverUrl.setScheme(lit("http"));
            serverUrl.setHost(remoteAddr);
            serverUrl.setPort(info.port);
            serverUrl.setUserName(userName);
            serverUrl.setPassword(password);
            availableServers << serverUrl;
            ++hostsCount[serverUrl.host()];
        }
    }    

    if (availableServers.isEmpty()) {
        qWarning() << "No servers are available for reconnect" << allServers << "modules:" << QnModuleFinder::instance()->foundModules().size();
        availableServers << currentUrl;
    }

    qSort(availableServers.begin(), availableServers.end(), hostLessThan);

    auto getInfo = [availableServers, hostsCount](const QUrl &url) {
        QStringList result;
        foreach (const QUrl &serverUrl, availableServers) {
            QString info = serverUrl.host();
            if (hostsCount[info] > 1)
                info = lit("%1:%2").arg(info).arg(serverUrl.port());
            if (url.host() == serverUrl.host() && url.port() == serverUrl.port())
                info = lit("<b>%1</b>").arg(info);
            result << info;
        }
        return result.join(lit("<br>"));
    };

    /* Main window can be closed in the event loop so the dialog will be freed. */
    QPointer<QnReconnectInfoDialog> dialogGuard(reconnectInfoDialog);

    /* Here we will wait for the reconnect or cancel. */
    auto iter = availableServers.cbegin();
    while (dialogGuard && !reconnectInfoDialog->wasCanceled()) {
        reconnectInfoDialog->setText(getInfo(*iter));
        if (connectToServer(*iter, true))   /* Here inner event loop will be started. */
            break;
        ++iter;
        if (iter == availableServers.cend())
            iter = availableServers.cbegin();   //loop
    }

    if (!dialogGuard)
        return true;

    disconnect(QnClientMessageProcessor::instance(), NULL, reconnectInfoDialog, NULL);
    reconnectInfoDialog->deleteLater();
    if (reconnectInfoDialog->wasCanceled())
        return false;
    return true;
}


void QnWorkbenchConnectHandler::at_beforeExitAction_triggered() {
    disconnectFromServer(true);

    if (loginDialog())
        delete loginDialog();
}


