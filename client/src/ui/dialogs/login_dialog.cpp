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

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "plugins/resources/archive/filetypesupport.h"

#include <client/client_settings.h>

#include "connection_testing_dialog.h"

#include "connectinfo.h"
#include "compatibility_version_installation_dialog.h"
#include "version.h"
#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"


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
    m_entCtrlFinder(NULL),
    m_restartPending(false),
    m_autoConnectPending(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::Login_Help);

    /* Don't allow to save passwords, at least for now. */
    //ui->savePasswordCheckBox->hide();

    QDir dir(QLatin1String(":/skin"));
    QStringList introList = dir.entryList(QStringList() << QLatin1String("intro.*"));
    QString resourceName = QLatin1String(":/skin/intro");
    if (!introList.isEmpty())
        resourceName = QLatin1String(":/skin/") + introList.first();

    QnAviResourcePtr resource = QnAviResourcePtr(new QnAviResource(QLatin1String("qtfile://") + resourceName));
    if (FileTypeSupport::isImageFileExt(resourceName))
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

    m_entCtrlFinder = new NetworkOptixModuleFinder();
    if (qnSettings->isDevMode())
        m_entCtrlFinder->setCompatibilityMode(true);
    connect(m_entCtrlFinder,    SIGNAL(moduleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)),
            this,               SLOT(at_entCtrlFinder_remoteModuleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)));
    connect(m_entCtrlFinder,    SIGNAL(moduleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)),
            this,               SLOT(at_entCtrlFinder_remoteModuleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)));
    m_entCtrlFinder->start();
}

QnLoginDialog::~QnLoginDialog() {
    delete m_entCtrlFinder;
    return;
}

void QnLoginDialog::updateFocus() 
{
    ui->passwordLineEdit->setFocus();
}

QUrl QnLoginDialog::currentUrl() const {
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordLineEdit->text().trimmed());
    return url;
}

QString QnLoginDialog::currentName() const {
    return ui->connectionsComboBox->currentText();
}

QnConnectInfoPtr QnLoginDialog::currentInfo() const {
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

    QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(url);
    m_requestHandle = connection->connectAsync(this, SLOT(at_connectFinished(int, QnConnectInfoPtr, int)));

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
    if (m_autoConnectPending
            && ui->rememberPasswordCheckBox->isChecked()
            && !ui->passwordLineEdit->text().isEmpty()
            && currentUrl().isValid())
        accept();
    else
        resetConnectionsModel();
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
    m_autoFoundItem->removeRows(0, m_autoFoundItem->rowCount());
    if (m_foundEcs.size() == 0) {
        QStandardItem* noLocalEcs = new QStandardItem(tr("<none>"));
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_autoFoundItem->appendRow(noLocalEcs);
    } else {
        foreach (QUrl url, m_foundEcs) {
            QStandardItem* item = new QStandardItem(url.host() + QLatin1Char(':') + QString::number(url.port()));
            item->setData(url, Qn::UrlRole);
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
void QnLoginDialog::at_connectFinished(int status, QnConnectInfoPtr connectInfo, int requestHandle) {
    if(m_requestHandle != requestHandle) 
        return;
    m_requestHandle = -1;

    updateUsability();

    bool compatibleProduct = qnSettings->isDevMode() || connectInfo->brand.isEmpty()
            || connectInfo->brand == QLatin1String(QN_PRODUCT_NAME_SHORT);
    bool success = (status == 0) && compatibleProduct;

    QString detail;

    if (status == 202) {
        detail = tr("Login or password you have entered are incorrect, please try again.");
    } else if (status != 0) {
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

    QnCompatibilityChecker remoteChecker(connectInfo->compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectInfo->version)) {
        QnSoftwareVersion minSupportedVersion("1.4"); 

        m_restartPending = true;
        if (connectInfo->version < minSupportedVersion) {
            QnMessageBox::warning(
                this,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Enterprise Controller"),
                tr("You are about to connect to Enterprise Controller which has a different version:\n"
                   " - Client version: %1.\n"
                   " - EC version: %2.\n"
                   "Compatibility mode for versions lower than %3 is not supported.")
                    .arg(QLatin1String(QN_ENGINE_VERSION))
                    .arg(connectInfo->version.toString())
                    .arg(minSupportedVersion.toString())
            );
            m_restartPending = false;
        }

        if(m_restartPending) {
            for( ;; )
            {
                bool isInstalled = false;
                if( applauncher::isVersionInstalled(connectInfo->version, &isInstalled) != applauncher::api::ResultType::ok )
                {
                    QnMessageBox::warning(
                        this,
                        Qn::VersionMismatch_Help,
                        tr("Could not connect to Enterprise Controller"),
                        tr("You are about to connect to Enterprise Controller which has a different version:\n"
                            " - Client version: %1.\n"
                            " - EC version: %2.\n"
                            "Unable to connect to applauncher to enable client compatibility mode."
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectInfo->version.toString()),
                        QMessageBox::Ok
                    );
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
                            "Would you like to restart client in compatibility mode?"
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectInfo->version.toString()),
                        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel), 
                        QMessageBox::Cancel
                    );
                    if(button == QMessageBox::Ok) {
                        if (applauncher::restartClient(connectInfo->version, currentUrl().toEncoded()) != applauncher::api::ResultType::ok) {
                            QMessageBox::critical(
                                        this,
                                        tr("Launcher process is not found"),
                                        tr("Cannot restart the client in compatibility mode.\n"
                                           "Please close the application and start it again using the shortcut in the start menu.")
                                        );
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
                            "Client Version %3 is required to connect to this Enterprise Controller.\n"
                            "Download version %3?"
                        ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectInfo->version.toString()).arg(connectInfo->version.toString(QnSoftwareVersion::MinorFormat)),
                        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                        QMessageBox::Cancel
                    );
                    if( selectedButton == QMessageBox::Ok ) {
                        //starting installation
                        if( !m_installationDialog.get() )
                            m_installationDialog.reset( new CompatibilityVersionInstallationDialog( this ) );
                        m_installationDialog->setVersionToInstall( connectInfo->version );
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

    m_connectInfo = connectInfo;
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

void QnLoginDialog::at_entCtrlFinder_remoteModuleFound(const QString& moduleID, const QString& moduleVersion, const TypeSpecificParamMap& moduleParameters, const QString& localInterfaceAddress, const QString& remoteHostAddress, bool isLocal, const QString& seed) {
    Q_UNUSED(moduleVersion)
    Q_UNUSED(localInterfaceAddress)

    QString portId = QLatin1String("port");

    if (moduleID != nxEntControllerId ||  !moduleParameters.contains(portId))
        return;

    QString host = isLocal ? QString::fromLatin1("127.0.0.1") : remoteHostAddress;
    QUrl url;
    url.setHost(host);

    QString port = moduleParameters[portId];
    url.setPort(port.toInt());

    QMultiHash<QString, QUrl>::iterator i = m_foundEcs.find(seed);
    while (i != m_foundEcs.end() && i.key() == seed) {
        QUrl found = i.value();
        if (found.host() == url.host() && found.port() == url.port())
            return; // found the same host, e.g. two interfaces on local controller
        ++i;
    }
    m_foundEcs.insert(seed, url);
    resetAutoFoundConnectionsModel();
}

void QnLoginDialog::at_entCtrlFinder_remoteModuleLost(const QString& moduleID, const TypeSpecificParamMap& moduleParameters, const QString& remoteHostAddress, bool isLocal, const QString& seed) {
    Q_UNUSED(moduleParameters)
    Q_UNUSED(remoteHostAddress)
    Q_UNUSED(isLocal)

    if( moduleID != nxEntControllerId )
        return;
    m_foundEcs.remove(seed);

    resetAutoFoundConnectionsModel();
}
