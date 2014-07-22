#include "workbench_connect_handler.h"

#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>
#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_connection_data.h>
#include <client/client_message_processor.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>

#include <ui/dialogs/login_dialog.h>

#include <ui/graphics/items/generic/graphics_message_box.h>

#include <ui/style/skin.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_layouts_handler.h>            //TODO: #GDM dependencies

#include <utils/app_server_notification_cache.h>
#include <utils/common/software_version.h>

#include "compatibility.h"
#include "version.h"

namespace {
    const int videowallReconnectTimeoutMSec = 5000;
    const int videowallCloseTimeoutMSec = 10000;

    const int maxReconnectTimeout = 10*1000;                // 10 seconds
    const int maxVideowallReconnectTimeout = 96*60*60*1000; // 4 days
}

QnWorkbenchConnectHandler::QnWorkbenchConnectHandler(QObject *parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_connectingMessageBox(NULL)
{
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionOpened,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionOpened);
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::connectionClosed,    this,   &QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed);

    connect(action(Qn::ConnectAction),              &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_connectAction_triggered);
    connect(action(Qn::ReconnectAction),            &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_reconnectAction_triggered);
    connect(action(Qn::DisconnectAction),           &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::at_disconnectAction_triggered);

    connect(action(Qn::OpenLoginDialogAction),      &QAction::triggered,                            this,   &QnWorkbenchConnectHandler::showLoginDialog);

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

    if (m_connectingMessageBox != NULL) {
        m_connectingMessageBox->disconnect(this);
        m_connectingMessageBox->hideImmideately();
        m_connectingMessageBox = NULL;
    }

    connection2()->sendRuntimeData(QnRuntimeInfoManager::instance()->localInfo().data);
}

void QnWorkbenchConnectHandler::at_messageProcessor_connectionClosed() {
    /* Don't do anything if we are closing client. */
    if (!mainWindow())
        return;

    if (qnSettings->isVideoWallMode()) {
        m_connectingMessageBox = QnGraphicsMessageBox::information(
            tr("Connection failed. Trying to restore connection..."),
            maxVideowallReconnectTimeout);   

        connect(m_connectingMessageBox, &QnGraphicsMessageBox::finished, this, [this] {
            m_connectingMessageBox = NULL;
            menu()->trigger(Qn::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));
            menu()->trigger(Qn::ExitAction);
        });
        return;
    }

    action(Qn::OpenLoginDialogAction)->setIcon(qnSkin->icon("titlebar/disconnected.png"));
    action(Qn::OpenLoginDialogAction)->setText(tr("Connect to Server..."));

    if (cameraAdditionDialog())
        cameraAdditionDialog()->hide();
    context()->instance<QnAppServerNotificationCache>()->clear();

    menu()->trigger(Qn::ClearCameraSettingsAction);

    /* Remove all remote resources. */
    const QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(QnResource::remote);
    resourcePool()->removeResources(remoteResources);

    /* Also remove layouts that were just added and have no 'remote' flag set. */
    foreach(const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>())
        if(snapshotManager()->isLocal(layout))
            resourcePool()->removeResource(layout);

    qnLicensePool->reset();
    notificationsHandler()->clear();

    /* If we were not disconnected intentionally show corresponding message. */
    if (!qnCommon->remoteGUID().isNull()) {
        m_connectingMessageBox = QnGraphicsMessageBox::informationTicking(
            tr("Connection failed. Trying to restore connection... %1"),
            maxReconnectTimeout);

        connect(m_connectingMessageBox, &QnGraphicsMessageBox::finished, this, [this]{
            menu()->trigger(Qn::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));
            menu()->trigger(Qn::OpenLoginDialogAction);
            m_connectingMessageBox = NULL;
        });
    }
}

void QnWorkbenchConnectHandler::at_connectAction_triggered() {
    Q_ASSERT(!connected());
    if (connected())
        return;

    QnActionParameters parameters = menu()->currentParameters(sender());
    QUrl url = parameters.argument(Qn::UrlRole, QUrl());

    if (url.isValid()) {
        /* Videowall item */
        if (qnSettings->isVideoWallMode()) {
            if (!connectToServer(url)) {
                QnGraphicsMessageBox* incompatibleMessageBox = QnGraphicsMessageBox::informationTicking(tr("Incompatible server. Closing in %1..."), videowallCloseTimeoutMSec);
                connect(incompatibleMessageBox, &QnGraphicsMessageBox::finished, action(Qn::ExitAction), &QAction::trigger);
            }
        } 
        else
        /* 'Open in new window' while connected */
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
        if (url.isValid() && !qnSettings->storedPassword().isEmpty()) {
            url.setPassword(qnSettings->storedPassword());
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
    if (connected())
        disconnectFromServer(true);
    if (!connectToServer(QnAppServerConnectionFactory::url()))
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
    return !qnCommon->moduleGUID().isNull() && context()->user();
}

bool QnWorkbenchConnectHandler::connectToServer(const QUrl &appServerUrl) {
    Q_ASSERT(!connected());
    if (connected())
        return;

    QnEc2ConnectionRequestResult result;
    QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(appServerUrl, &result, &QnEc2ConnectionRequestResult::processEc2Reply );

    QnGraphicsMessageBox* connectingMessageBox = QnGraphicsMessageBox::information(tr("Connecting..."), INT_MAX);

    //here we are going to inner event loop
    ec2::ErrorCode errCode = static_cast<ec2::ErrorCode>(result.exec());
    connectingMessageBox->hideImmideately();

    QnConnectionInfo connectionInfo = result.reply<QnConnectionInfo>();
    if (!checkCompatibility(connectionInfo, errCode))
        return false;

    QnAppServerConnectionFactory::setUrl(appServerUrl);
    QnAppServerConnectionFactory::setEc2Connection(result.connection());
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnCommonMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());

    QnSessionManager::instance()->start();
    QnResource::startCommandProc();

    context()->setUserName(appServerUrl.userName());
}

bool QnWorkbenchConnectHandler::disconnectFromServer(bool force) {
    if (!saveState(force))
        return false;

    if (!force)
        qnSettings->setStoredPassword(QString());

    if (m_connectingMessageBox != NULL) {
        m_connectingMessageBox->disconnect(this);
        m_connectingMessageBox->hideImmideately();
        m_connectingMessageBox = NULL;
    }

    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());

    QnCommonMessageProcessor::instance()->init(NULL);

    QnSessionManager::instance()->stop();
    QnResource::stopCommandProc();

    context()->setUserName(QString());

    return true;
}

void QnWorkbenchConnectHandler::showLoginDialog() {
    if (!loginDialog()) {
        m_loginDialog = new QnLoginDialog(mainWindow(), context());
        loginDialog()->setModal(true);
    }

    QUrl url;
    // show login dialog again and again till success or cancel
    while (true) {
        if(!loginDialog()->exec())
            return;

        url = loginDialog()->currentUrl();
        if (!url.isValid()) 
            continue;   //TODO: #GDM show message? dialog should validate url itself

    //     if (loginDialog()->restartPending())
    //         QTimer::singleShot(10, this, SLOT(at_exitAction_triggered()));

        // ask user if he wants to save changes
        if (connected() && !disconnectFromServer(false))
            return; 

        if (connectToServer(url))
            break;
    }

    QnConnectionDataList connections = qnSettings->customConnections();

    QnConnectionData connectionData(QString(), url);
    qnSettings->setLastUsedConnection(connectionData);

    qnSettings->setStoredPassword(loginDialog()->rememberPassword()
        ? url.password()
        : QString()
        );

    // remove previous "Last used connection"
    connections.removeOne(QnConnectionDataList::defaultLastUsedNameKey());

    QUrl cleanUrl(connectionData.url);
    cleanUrl.setPassword(QString());
    QnConnectionData selected = connections.getByName(loginDialog()->currentName());
    if (selected.url == cleanUrl){
        connections.removeOne(selected.name);
        connections.prepend(selected);
    } else {
        // save "Last used connection"
        QnConnectionData last(connectionData);
        last.name = QnConnectionDataList::defaultLastUsedNameKey();
        last.url.setPassword(QString());
        connections.prepend(last);
    }
    qnSettings->setCustomConnections(connections);
}

bool QnWorkbenchConnectHandler::saveState(bool force) {
    if (!connected())
        return true;

    QnWorkbenchState state;
    workbench()->submit(state);

    //closeAllLayouts should return true in case of force disconnect
    if(!context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true, force)) 
        return false;

    QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
    states[context()->user()->getName()] = state;
    qnSettings->setUserWorkbenchStates(states);
}

bool QnWorkbenchConnectHandler::checkCompatibility(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode) {
   
    bool success = (errorCode == ec2::ErrorCode::ok);

    if (success)
        //checking compatibility
        success |= qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QLatin1String(QN_PRODUCT_NAME_SHORT);

    QString detail;

    if (errorCode == ec2::ErrorCode::unauthorized) {
        detail = tr("Login or password you have entered are incorrect, please try again.");
    } else if (errorCode != ec2::ErrorCode::ok) {
        detail = tr("Connection to the Enterprise Controller could not be established.\n"
            "Connection details that you have entered are incorrect, please try again.\n\n"
            "If this error persists, please contact your VMS administrator.");
    } else {
        detail = tr("You are trying to connect to incompatible Enterprise Controller.");
    }

    if(!success) {
        QnMessageBox::warning(
            this,
            Qn::Login_Help,
            tr("Could not connect to Enterprise Controller"),
            detail
            );
        return false;
    }

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker *compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectionInfo.version)) {
        QnSoftwareVersion minSupportedVersion("1.4"); 

        m_restartPending = true;
        if (connectionInfo.version < minSupportedVersion) {
            QnMessageBox::warning(
                this,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Enterprise Controller"),
                tr("You are about to connect to Enterprise Controller which has a different version:\n"
                " - Client version: %1.\n"
                " - EC version: %2.\n"
                "Compatibility mode for versions lower than %3 is not supported."
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()).arg(minSupportedVersion.toString()),
                QMessageBox::Ok
                );
            m_restartPending = false;
        }

        if (connectionInfo.version > QnSoftwareVersion(QN_ENGINE_VERSION)) {
#ifndef Q_OS_MACX
            QnMessageBox::warning(
                this,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Enterprise Controller"),
                tr("Selected Enterprise controller has a different version:\n"
                " - Client version: %1.\n"
                " - EC version: %2.\n"
                "An error has occurred while trying to restart in compatibility mode."
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                QMessageBox::Ok
                );
#else
            QnMessageBox::warning(
                this,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Enterprise Controller"),
                tr("Selected Enterprise controller has a different version:\n"
                " - Client version: %1.\n"
                " - EC version: %2.\n"
                "The other version of client is needed in order to establish the connection to this server."
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                QMessageBox::Ok
                );
#endif
            m_restartPending = false;
        }

        if(m_restartPending) {
            for (;;) {
                bool isInstalled = false;
                if (applauncher::isVersionInstalled(connectionInfo.version, &isInstalled) != applauncher::api::ResultType::ok)
                {
#ifndef Q_OS_MACX
                    QnMessageBox::warning(
                        this,
                        Qn::VersionMismatch_Help,
                        tr("Could not connect to Enterprise Controller"),
                        tr("Selected Enterprise controller has a different version:\n"
                        " - Client version: %1.\n"
                        " - EC version: %2.\n"
                        "An error has occurred while trying to restart in compatibility mode."
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                        QMessageBox::Ok
                        );
#else
                    QnMessageBox::warning(
                        this,
                        Qn::VersionMismatch_Help,
                        tr("Could not connect to Enterprise Controller"),
                        tr("Selected Enterprise controller has a different version:\n"
                        " - Client version: %1.\n"
                        " - EC version: %2.\n"
                        "The other version of client is needed in order to establish the connection to this server."
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                        QMessageBox::Ok
                        );
#endif
                    m_restartPending = false;
                }
                else if(isInstalled) {
                    int button = QnMessageBox::warning(
                        this,
                        Qn::VersionMismatch_Help,
                        tr("Could not connect to Enterprise Controller"),
                        tr("You are about to connect to Enterprise Controller which has a different version:\n"
                        " - Client version: %1.\n"
                        " - EC version: %2.\n"
                        "Would you like to restart in compatibility mode?"
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel), 
                        QMessageBox::Cancel
                        );
                    if(button == QMessageBox::Ok) {
                        switch( applauncher::restartClient(connectionInfo.version, currentUrl().toEncoded()) )
                        {
                        case applauncher::api::ResultType::ok:
                            break;

                        case applauncher::api::ResultType::connectError:
                            QMessageBox::critical(
                                this,
                                tr("Launcher process is not found"),
                                tr("Cannot restart the client in compatibility mode.\n"
                                "Please close the application and start it again using the shortcut in the start menu.")
                                );
                            break;

                        default:
                            {
                                //trying to restore installation
                                int selectedButton = QnMessageBox::warning(
                                    this,
                                    Qn::VersionMismatch_Help,
                                    tr("Failure"),
                                    tr("Failed to launch compatibility version %1\n"
                                    "Try to restore version %1?").
                                    arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                                    QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                                    QMessageBox::Cancel
                                    );
                                if( selectedButton == QMessageBox::Ok ) {
                                    //starting installation
                                    if( !m_installationDialog.get() )
                                        m_installationDialog.reset( new CompatibilityVersionInstallationDialog( this ) );
                                    m_installationDialog->setVersionToInstall( connectionInfo.version );
                                    m_installationDialog->exec();
                                    if( m_installationDialog->installationSucceeded() )
                                        continue;   //offering to start newly-installed compatibility version
                                }
                                m_restartPending = false;
                                break;
                            }
                        }
                    } else {
                        m_restartPending = false;
                    }
                } else {    //not installed
                    int selectedButton = QnMessageBox::warning(
                        this,
                        Qn::VersionMismatch_Help,
                        tr("Could not connect to Enterprise Controller"),
                        tr("You are about to connect to Enterprise Controller which has a different version:\n"
                        " - Client version: %1.\n"
                        " - EC version: %2.\n"
                        "Client version %3 is required to connect to this Enterprise Controller.\n"
                        "Download version %3?"
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()).arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                        QMessageBox::Cancel
                        );
                    if( selectedButton == QMessageBox::Ok ) {
                        //starting installation
                        if( !m_installationDialog.get() )
                            m_installationDialog.reset( new CompatibilityVersionInstallationDialog( this ) );
                        m_installationDialog->setVersionToInstall( connectionInfo.version );
                        m_installationDialog->exec();
                        if( m_installationDialog->installationSucceeded() )
                            continue;   //offering to start newly-installed compatibility version
                    }
                    m_restartPending = false;
                }
                break;
            }
        }

        if (!m_restartPending) {
            return;
        }
    }
}

