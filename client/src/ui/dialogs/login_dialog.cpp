#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QInputDialog>
#include <QtGui/QStandardItemModel>

#include <client/client_connection_data.h>

#include <core/resource/resource.h>

#include <api/app_server_connection.h>
#include <api/session_manager.h>

#include <ui/dialogs/preferences_dialog.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "plugins/resources/archive/filetypesupport.h"

#include <utils/settings.h>

#include "connection_testing_dialog.h"

#include "connectinfo.h"
#include "version.h"
#include "ui/graphics/items/resource/decodedpicturetoopengluploadercontextpool.h"


namespace {
    void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled) {
        foreach(QObject *object, objects)
            if(object != exclude)
                if(QWidget *widget = qobject_cast<QWidget *>(object))
                    widget->setEnabled(enabled);
    }

    static QString space = QLatin1String ("    ");

} // anonymous namespace


LoginDialog::LoginDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog),
    m_context(context),
    m_requestHandle(-1),
    m_renderingWidget(NULL)
{
    if(!context)
        qnNullWarning(context);

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

    m_renderingWidget = new QnRenderingWidget();
    m_renderingWidget->setResource(resource);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);
    layout->addWidget(m_renderingWidget);
    DecodedPictureToOpenGLUploaderContextPool::instance()->ensureThereAreContextsSharedWith( m_renderingWidget );

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    connect(ui->connectionsComboBox,        SIGNAL(currentIndexChanged(int)),       this,   SLOT(at_connectionsComboBox_currentIndexChanged(int)));
    connect(ui->testButton,                 SIGNAL(clicked()),                      this,   SLOT(at_testButton_clicked()));
    connect(ui->saveButton,                 SIGNAL(clicked()),                      this,   SLOT(at_saveButton_clicked()));
    connect(ui->deleteButton,               SIGNAL(clicked()),                      this,   SLOT(at_deleteButton_clicked()));
    connect(ui->passwordLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->loginLineEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->hostnameLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->portSpinBox,                SIGNAL(valueChanged(int)),              this,   SLOT(updateAcceptibility()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));

    m_dataWidgetMapper = new QDataWidgetMapper(this);
    m_dataWidgetMapper->setModel(m_connectionsModel);
    m_dataWidgetMapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    m_dataWidgetMapper->setOrientation(Qt::Horizontal);
    m_dataWidgetMapper->addMapping(ui->hostnameLineEdit, 1);
    m_dataWidgetMapper->addMapping(ui->portSpinBox, 2);
    m_dataWidgetMapper->addMapping(ui->loginLineEdit, 3);

    resetConnectionsModel();
    updateFocus();

    m_entCtrlFinder = new NetworkOptixModuleFinder();
    connect(m_entCtrlFinder,    SIGNAL(moduleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)),
            this,               SLOT(at_entCtrlFinder_remoteModuleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)));
    connect(m_entCtrlFinder,    SIGNAL(moduleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)),
            this,               SLOT(at_entCtrlFinder_remoteModuleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)));
    m_entCtrlFinder->start();
}

LoginDialog::~LoginDialog() {
    delete m_entCtrlFinder;
    return;
}

void LoginDialog::updateFocus() 
{
    ui->passwordLineEdit->setFocus();
}

QUrl LoginDialog::currentUrl() const {
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(ui->hostnameLineEdit->text().trimmed());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text().trimmed());
    url.setPassword(ui->passwordLineEdit->text().trimmed());
    return url;
}

QString LoginDialog::currentName() const {
    QString itemText = ui->connectionsComboBox->itemText(ui->connectionsComboBox->currentIndex());
    if (itemText.startsWith(space))
        return itemText.remove(0, space.length());
    return itemText;
}

QnConnectInfoPtr LoginDialog::currentInfo() const {
    return m_connectInfo;
}

void LoginDialog::accept() {
    QUrl url = currentUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(url);
    m_requestHandle = connection->connectAsync(this, SLOT(at_connectFinished(int, const QByteArray &, QnConnectInfoPtr, int)));

    updateUsability();
}

void LoginDialog::reject() {
    if(m_requestHandle == -1) {
        QDialog::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void LoginDialog::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void LoginDialog::resetConnectionsModel() {
    m_connectionsModel->removeRows(0, m_connectionsModel->rowCount());

    QnConnectionDataList connections = qnSettings->customConnections();
    if (connections.isEmpty())
        connections.append(qnSettings->defaultConnection());

    int selectedIndex = -1;

    QnConnectionData lastUsed = connections.getByName(QnConnectionDataList::defaultLastUsedNameKey());
    if (lastUsed != QnConnectionData()) {
        QList<QStandardItem *> row;
        row << new QStandardItem(QnConnectionDataList::defaultLastUsedName())
            << new QStandardItem(lastUsed.url.host())
            << new QStandardItem(QString::number(lastUsed.url.port()))
            << new QStandardItem(lastUsed.url.userName());
        m_connectionsModel->appendRow(row);
        selectedIndex = 0;
    }

    QStandardItem *headerSavedItem = new QStandardItem(tr("Saved Sessions"));
    headerSavedItem->setFlags(Qt::ItemIsEnabled);
    m_connectionsModel->appendRow(headerSavedItem);

    foreach (const QnConnectionData &connection, connections) {
        if (connection.name == QnConnectionDataList::defaultLastUsedNameKey())
            continue;

        QList<QStandardItem *> row;
        row << new QStandardItem(space + connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName());
        m_connectionsModel->appendRow(row);
        if (selectedIndex < 0)
            selectedIndex = 1; // skip header row
    }

    QStandardItem* headerFoundItem = new QStandardItem(tr("Auto-Discovered ECs"));
    headerFoundItem->setFlags(Qt::ItemIsEnabled);
    m_connectionsModel->appendRow(headerFoundItem);
    resetAutoFoundConnectionsModel();
    ui->connectionsComboBox->setCurrentIndex(selectedIndex); /* Last used connection if exists, else last saved connection. */
    ui->passwordLineEdit->clear();

}

void LoginDialog::resetAutoFoundConnectionsModel() {
    QnConnectionDataList connections = qnSettings->customConnections();

    int baseCount = qMax(connections.size(), 1); //1 for default auto-created connection
    baseCount += 2; //headers

    int count = m_connectionsModel->rowCount() - baseCount;
    if (count > 0)
        m_connectionsModel->removeRows(baseCount, count);

    if (m_foundEcs.size() == 0) {
        QStandardItem* noLocalEcs = new QStandardItem(space + tr("<none>"));
        noLocalEcs->setFlags(Qt::ItemIsEnabled);
        m_connectionsModel->appendRow(noLocalEcs);
    } else {
        foreach (QUrl url, m_foundEcs) {
            QList<QStandardItem *> row;
            row << new QStandardItem(space + url.host() + QLatin1Char(':') + QString::number(url.port()))
                << new QStandardItem(url.host())
                << new QStandardItem(QString::number(url.port()))
                << new QStandardItem(QString());
            m_connectionsModel->appendRow(row);
        }
    }

}

void LoginDialog::updateAcceptibility() {
    bool acceptable = 
        !ui->passwordLineEdit->text().isEmpty() &&
        !ui->loginLineEdit->text().trimmed().isEmpty() &&
        !ui->hostnameLineEdit->text().trimmed().isEmpty() &&
        ui->portSpinBox->value() != 0;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
    ui->testButton->setEnabled(acceptable);
}

void LoginDialog::updateUsability() {
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

void LoginDialog::at_connectFinished(int status, const QByteArray &/*errorString*/, QnConnectInfoPtr connectInfo, int requestHandle) {
    if(m_requestHandle != requestHandle) 
        return;
    m_requestHandle = -1;

    updateUsability();

    if(status != 0) {
        QMessageBox::warning(
            this, 
            tr("Could not connect to Enterprise Controller"), 
            tr("Connection to the Enterprise Controller could not be established.\nConnection details that you have entered are incorrect, please try again.\n\nIf this error persists, please contact your VMS administrator.")
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

    if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QLatin1String(QN_ENGINE_VERSION), QLatin1String("ECS"), connectInfo->version)) {
        QMessageBox::warning(
            this,
            tr("Could not connect to Enterprise Controller"),
            tr("Connection could not be established.\nThe Enterprise Controller is incompatible with this client. Please upgrade your client or contact your VMS administrator.")
        );
        updateFocus();
        return;
    }

    m_connectInfo = connectInfo;
    QDialog::accept();
}

void LoginDialog::at_connectionsComboBox_currentIndexChanged(int index) {
    QModelIndex idx = m_connectionsModel->index(index, 0);
    m_dataWidgetMapper->setCurrentModelIndex(idx);
    ui->passwordLineEdit->clear();
    updateFocus();
}

void LoginDialog::at_testButton_clicked() {
    QUrl url = currentUrl();

    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("The information you have entered is not valid."));
        return;
    }

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(url, this));
    dialog->setModal(true);
    dialog->exec();

    updateFocus();
}

void LoginDialog::at_saveButton_clicked() {
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
                                                                  tr("Connection with the same name already exists. Overwrite it?"),
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

    resetConnectionsModel();

    int idx = 1;
    if (connections.contains(QnConnectionDataList::defaultLastUsedNameKey()))
        idx++;
    ui->connectionsComboBox->setCurrentIndex(idx);
    ui->passwordLineEdit->setText(password);

}

void LoginDialog::at_deleteButton_clicked() {
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

void LoginDialog::at_entCtrlFinder_remoteModuleFound(const QString& moduleID, const QString& moduleVersion, const TypeSpecificParamMap& moduleParameters, const QString& localInterfaceAddress,
    const QString& remoteHostAddress, bool isLocal, const QString& seed) {
    Q_UNUSED(moduleVersion)
    Q_UNUSED(localInterfaceAddress)

    QString portId = QLatin1String("port");

    if (moduleID != nxEntControllerId ||  !moduleParameters.contains(portId))
        return;

    QString host = isLocal ? QString::fromAscii("127.0.0.1") : remoteHostAddress;
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

void LoginDialog::at_entCtrlFinder_remoteModuleLost(const QString& moduleID, const TypeSpecificParamMap& moduleParameters, const QString& remoteHostAddress, bool isLocal, const QString& seed ) {
    Q_UNUSED(moduleParameters)
    Q_UNUSED(remoteHostAddress)
    Q_UNUSED(isLocal)

    if( moduleID != nxEntControllerId )
        return;
    m_foundEcs.remove(seed);

    resetAutoFoundConnectionsModel();
}
