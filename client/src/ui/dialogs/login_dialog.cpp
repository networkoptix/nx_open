#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtCore/QDir>

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>

#include <client/client_connection_data.h>

#include <core/resource/resource.h>

#include <api/app_server_connection.h>
#include <api/session_manager.h>

#include <ui/dialogs/message_box.h>
#include <ui/dialogs/preferences_dialog.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/gl_widget_factory.h>

#include <utils/applauncher_utils.h>
#include <utils/network/module_finder.h>

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "plugins/resources/archive/filetypesupport.h"

#include <client/client_settings.h>
#include <common/common_module.h>

#include "connection_testing_dialog.h"

#include <utils/network/global_module_finder.h>
#include <utils/network/router.h>
#include <api/model/connection_info.h>
#include "compatibility_version_installation_dialog.h"
#include "version.h"
#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"
#include "compatibility.h"
#include "common/common_module.h"


namespace {
    void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled) {
        foreach(QObject *object, objects)
            if(object != exclude)
                if(QWidget *widget = qobject_cast<QWidget *>(object))
                    widget->setEnabled(enabled);
    }

    QStandardItem *newConnectionItem(QnConnectionData connection) {
        if (connection == QnConnectionData())
            return NULL;
        QStandardItem *result = new QStandardItem(connection.name);
        result->setData(connection.url, Qn::UrlRole);
        return result;
    }

} // anonymous namespace


QnLoginDialog::QnLoginDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::LoginDialog),
    m_requestHandle(-1),
    m_renderingWidget(NULL),
    m_restartPending(false),
    m_autoConnectPending(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::Login_Help);

    static const char *introNames[] = { "intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg", NULL };
    QString introPath;
    for(const char **introName = introNames; *introName != NULL; introName++) {
        introPath = qnSkin->path(*introName);
        if(!introPath.isEmpty())
            break;
    }

    QnAviResourcePtr resource = QnAviResourcePtr(new QnAviResource(lit("qtfile://") + introPath));
    if (FileTypeSupport::isImageFileExt(introPath))
        resource->addFlags(QnResource::still_image);

    m_renderingWidget = QnGlWidgetFactory::create<QnRenderingWidget>();
    m_renderingWidget->setResource(resource);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);
    layout->addWidget(m_renderingWidget);
    DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith( m_renderingWidget );

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    m_lastUsedItem = NULL;
    m_savedSessionsItem = new QStandardItem(tr("Saved Sessions"));
    m_savedSessionsItem->setFlags(Qt::ItemIsEnabled);
    m_autoFoundItem = new QStandardItem(tr("Auto-Discovered ECs"));
    m_autoFoundItem->setFlags(Qt::ItemIsEnabled);

    m_connectionsModel->appendRow(m_savedSessionsItem);
    m_connectionsModel->appendRow(m_autoFoundItem);

    connect(ui->connectionsComboBox,        SIGNAL(currentIndexChanged(QModelIndex)), this,   SLOT(at_connectionsComboBox_currentIndexChanged(QModelIndex)));
    connect(ui->testButton,                 SIGNAL(clicked()),                      this,   SLOT(at_testButton_clicked()));
    connect(ui->saveButton,                 SIGNAL(clicked()),                      this,   SLOT(at_saveButton_clicked()));
    connect(ui->deleteButton,               SIGNAL(clicked()),                      this,   SLOT(at_deleteButton_clicked()));
    connect(ui->passwordLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->loginLineEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->hostnameLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->portSpinBox,                SIGNAL(valueChanged(int)),              this,   SLOT(updateAcceptibility()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));

    resetConnectionsModel();
    updateFocus();

    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleFound,     this,   &QnLoginDialog::at_moduleFinder_moduleFound);
    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleLost,      this,   &QnLoginDialog::at_moduleFinder_moduleLost);

    foreach (const QnModuleInformation &moduleInformation, QnModuleFinder::instance()->foundModules())
        at_moduleFinder_moduleFound(moduleInformation, *moduleInformation.remoteAddresses.begin());
}

QnLoginDialog::~QnLoginDialog() {}

void QnLoginDialog::updateFocus() {
    ui->passwordLineEdit->setFocus();
}

QUrl QnLoginDialog::currentUrl() const {
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordLineEdit->text());
    return url;
}

QString QnLoginDialog::currentName() const {
    return ui->connectionsComboBox->currentText();
}

QnConnectionInfo QnLoginDialog::currentInfo() const {
    return m_connectInfo;
}

bool QnLoginDialog::restartPending() const {
    return m_restartPending;
}

void QnLoginDialog::accept() {
    QUrl url = currentUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The login information you have entered is not valid."));
        return;
    }

    QnAppServerConnectionFactory::ec2ConnectionFactory()->connect( url, this, &QnLoginDialog::at_ec2ConnectFinished );
    updateUsability();
}

bool QnLoginDialog::rememberPassword() const {
    return ui->rememberPasswordCheckBox->isChecked();
}

void QnLoginDialog::setAutoConnect(bool value) {
    m_autoConnectPending = value;
}

void QnLoginDialog::reject() {
    if(m_requestHandle == -1) {
        base_type::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void QnLoginDialog::changeEvent(QEvent *event) {
    base_type::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void QnLoginDialog::showEvent(QShowEvent *event) {
    base_type::showEvent(event);

    if (m_autoConnectPending && ui->rememberPasswordCheckBox->isChecked() && !ui->passwordLineEdit->text().isEmpty() && currentUrl().isValid()) {
        accept();
    } else {
        resetConnectionsModel();
    }

#ifdef Q_OS_MAC
    if (focusWidget())
        focusWidget()->activateWindow();
#endif
}

void QnLoginDialog::hideEvent(QHideEvent *event)
{
    base_type::hideEvent(event);
    m_renderingWidget->stopPlayback();
}

void QnLoginDialog::resetConnectionsModel() {
    QnConnectionDataList connections = qnSettings->customConnections();

    if (m_lastUsedItem != NULL)
        m_connectionsModel->removeRow(0); //last-used-connection row

    QModelIndex selectedIndex;
    m_lastUsedItem = newConnectionItem(connections.getByName(QnConnectionDataList::defaultLastUsedNameKey()));
    if (m_lastUsedItem != NULL) {
        m_lastUsedItem->setText(tr("* Last used connection *"));
        m_connectionsModel->insertRow(0, m_lastUsedItem);
        selectedIndex = m_connectionsModel->index(0, 0);
    }

    resetSavedSessionsModel();
    resetAutoFoundConnectionsModel();

    /* Last used connection if exists, else last saved connection. */
    if (!selectedIndex.isValid())
        selectedIndex = m_connectionsModel->index(0, 0, m_savedSessionsItem->index());
    ui->connectionsComboBox->setCurrentIndex(selectedIndex);
    at_connectionsComboBox_currentIndexChanged(selectedIndex);

    QString password = qnSettings->storedPassword();
    ui->passwordLineEdit->setText(password);
    ui->rememberPasswordCheckBox->setChecked(!password.isEmpty());
}

void QnLoginDialog::resetSavedSessionsModel() {
    QnConnectionDataList connections = qnSettings->customConnections();
    if (connections.isEmpty())
        connections.append(qnSettings->defaultConnection());

    m_savedSessionsItem->removeRows(0, m_savedSessionsItem->rowCount());
    foreach (const QnConnectionData &connection, connections) {
        if (connection.name == QnConnectionDataList::defaultLastUsedNameKey())
            continue;
        m_savedSessionsItem->appendRow(newConnectionItem(connection));
    }
}

void QnLoginDialog::resetAutoFoundConnectionsModel() {
    QnCompatibilityChecker checker(localCompatibilityItems());

    m_autoFoundItem->removeRows(0, m_autoFoundItem->rowCount());
    if (m_foundEcs.size() == 0) {
        QStandardItem* noLocalEcs = new QStandardItem(tr("<none>"));
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_autoFoundItem->appendRow(noLocalEcs);
    } else {
        foreach (const QnEcData& data, m_foundEcs) {
            QUrl url = data.url;

            QnSoftwareVersion ecVersion(data.version);
            bool isCompatible = checker.isCompatible(QLatin1String("Client"), qnCommon->engineVersion(),
                                                     QLatin1String("ECS"),    ecVersion);


            QString title;
            if (!data.systemName.isEmpty())
                title = lit("%1 - (%2:%3)").arg(data.systemName).arg(url.host()).arg(url.port());
            else
                title = lit("%1:%2").arg(url.host()).arg(url.port());
            if (!isCompatible)
                title += lit(" (v%3)").arg(ecVersion.toString(QnSoftwareVersion::MinorFormat));

            QStandardItem* item = new QStandardItem(title);
            item->setData(url, Qn::UrlRole);

            if (!isCompatible)
                item->setData(QColor(Qt::red), Qt::TextColorRole);
            m_autoFoundItem->appendRow(item);
        }
    }
}

void QnLoginDialog::updateAcceptibility() {
    bool acceptable = 
        !ui->passwordLineEdit->text().isEmpty() &&
        !ui->loginLineEdit->text().trimmed().isEmpty() &&
        !ui->hostnameLineEdit->text().trimmed().isEmpty() &&
        ui->portSpinBox->value() != 0;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
}

void QnLoginDialog::updateUsability() {
    if(m_requestHandle == -1) {
        ::setEnabled(children(), ui->buttonBox, true);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), true);
        unsetCursor();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->unsetCursor();

        updateAcceptibility();
    } else {
        ::setEnabled(children(), ui->buttonBox, false);
        ::setEnabled(ui->buttonBox->children(), ui->buttonBox->button(QDialogButtonBox::Cancel), false);
        setCursor(Qt::BusyCursor);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::ArrowCursor);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnLoginDialog::at_ec2ConnectFinished( int handle, ec2::ErrorCode errorCode, const ec2::AbstractECConnectionPtr &connection ) {
    updateUsability();

    QnConnectionInfo connectionInfo;
    if (connection)
        connectionInfo = connection->connectionInfo();

    bool success = connection && errorCode == ec2::ErrorCode::ok;
    if( success )
    {
        //checking compatibility
        success = qnSettings->isDevMode() || connectionInfo.brand.isEmpty()
                || connectionInfo.brand == QLatin1String(QN_PRODUCT_NAME_SHORT);
    }

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
        updateFocus();
        return;
    }

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker *compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    if (!compatibilityChecker->isCompatible(QLatin1String("Client"), qnCommon->engineVersion(), QLatin1String("ECS"), connectionInfo.version)) {
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
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(minSupportedVersion.toString()),
                QMessageBox::Ok
            );
            m_restartPending = false;
        }

        if (connectionInfo.version > qnCommon->engineVersion()) {
#ifndef Q_OS_MACX
            QnMessageBox::warning(
                this,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Enterprise Controller"),
                tr("Selected Enterprise controller has a different version:\n"
                    " - Client version: %1.\n"
                    " - EC version: %2.\n"
                    "An error has occurred while trying to restart in compatibility mode."
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
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
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo->version.toString()),
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
                        ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
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
                        ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo->version.toString()),
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
                        ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
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
                        ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
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
            updateFocus();
            return;
        }
    }

    QnAppServerConnectionFactory::setEc2Connection( connection );
    m_connectInfo = connectionInfo;
    qnCommon->setLocalSystemName(connectionInfo.systemName);
    QnGlobalModuleFinder::instance()->setConnection(connection);
    base_type::accept();
}

void QnLoginDialog::at_connectionsComboBox_currentIndexChanged(const QModelIndex &index) {
    QStandardItem* item = m_connectionsModel->itemFromIndex(index);
    QUrl url = item == NULL ? QUrl() : item->data(Qn::UrlRole).toUrl();
    ui->hostnameLineEdit->setText(url.host());
    ui->portSpinBox->setValue(url.port());
    ui->loginLineEdit->setText(url.userName());
    ui->passwordLineEdit->clear();
    ui->rememberPasswordCheckBox->setChecked(false);
    updateFocus();
}

void QnLoginDialog::at_testButton_clicked() {
    QUrl url = currentUrl();

    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("The information you have entered is not valid."));
        return;
    }

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(this));
    dialog->testEnterpriseController(url);
    dialog->exec();

    updateFocus();
}

void QnLoginDialog::at_saveButton_clicked() {
    QUrl url = currentUrl();

    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("Entered hostname is not valid."));
        ui->hostnameLineEdit->setFocus();
        return;
    }

    if (url.host().length() == 0) {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("Host field cannot be empty."));
        ui->hostnameLineEdit->setFocus();
        return;
    }

    QnConnectionDataList connections = qnSettings->customConnections();

    bool ok = false;

    QString name = tr("%1 at %2").arg(ui->loginLineEdit->text()).arg(ui->hostnameLineEdit->text());
    name = QInputDialog::getText(this, tr("Save connection as..."), tr("Enter name:"), QLineEdit::Normal, name, &ok);
    if (!ok)
        return;

    // save here because of the 'connections' field modifying
    QString password = ui->passwordLineEdit->text();

    if (connections.contains(name)){
        QMessageBox::StandardButton button = QMessageBox::warning(this, tr("Connection already exists"),
                                                                  tr("Connection with this name already exists. Do you want to overwrite it?"),
                                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                                                  QMessageBox::Yes);
        switch(button) {
        case QMessageBox::Cancel:
            return;
        case QMessageBox::No:
            name = connections.generateUniqueName(name);
            break;
        case QMessageBox::Yes:
            connections.removeOne(name);
            break;
        default:
            break;
        }
    }

    QnConnectionData connectionData(name, currentUrl());
    connectionData.url.setPassword(QString());
    connections.prepend(connectionData);
    qnSettings->setCustomConnections(connections);

    resetSavedSessionsModel();

    QModelIndex idx = m_connectionsModel->index(0, 0, m_savedSessionsItem->index());
    ui->connectionsComboBox->setCurrentIndex(idx);
    at_connectionsComboBox_currentIndexChanged(idx); // call directly in case index change will not work
    ui->passwordLineEdit->setText(password);         // password is cleared on index change

}

void QnLoginDialog::at_deleteButton_clicked() {
    QString name = currentName();

    if (QMessageBox::warning(this, tr("Delete connections"),
                                   tr("Are you sure you want to delete the connection\n%1?").arg(name),
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;

    QnConnectionDataList connections = qnSettings->customConnections();
    connections.removeOne(name);
    qnSettings->setCustomConnections(connections);
    resetConnectionsModel();
}

void QnLoginDialog::at_moduleFinder_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress) {
    //if (moduleID != nxEntControllerId ||  !moduleParameters.contains(portId))
    //    return;

    QString host = moduleInformation.isLocal ? QLatin1String("127.0.0.1") : remoteAddress;
    QUrl url;
    url.setHost(host);
    url.setPort(moduleInformation.port);

    QnEcData data;
    data.id = moduleInformation.id;
    data.url = url;
    data.version = moduleInformation.version.toString();
    data.systemName = moduleInformation.systemName;
    QString key = data.systemName + url.toString();
    if (m_foundEcs.value(key) == data)
        return;

    m_foundEcs.insert(key, data);
    resetAutoFoundConnectionsModel();
}

void QnLoginDialog::at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation) {
    if (moduleInformation.type != nxMediaServerId)
        return;

    for(auto itr = m_foundEcs.begin(); itr != m_foundEcs.end(); ++itr)
    {
        if (itr.value().id == moduleInformation.id) {
            m_foundEcs.erase(itr);
            break;
        }
    }
    resetAutoFoundConnectionsModel();
}
