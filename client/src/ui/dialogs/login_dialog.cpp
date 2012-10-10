#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QInputDialog>
#include <QtGui/QStandardItemModel>

#include <utils/settings.h>
#include <core/resource/resource.h>
#include <api/app_server_connection.h>
#include <api/session_manager.h>

#include <ui/dialogs/preferences_dialog.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "plugins/resources/archive/filetypesupport.h"

#include "connection_testing_dialog.h"

#include "connectinfo.h"

namespace {
    void setEnabled(const QObjectList &objects, QObject *exclude, bool enabled) {
        foreach(QObject *object, objects)
            if(object != exclude)
                if(QWidget *widget = qobject_cast<QWidget *>(object))
                    widget->setEnabled(enabled);
    }

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
}

LoginDialog::~LoginDialog() {
    return;
}

void LoginDialog::updateFocus() 
{
    ui->passwordLineEdit->setFocus();
}

QUrl LoginDialog::currentUrl() const {
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(ui->hostnameLineEdit->text());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text());
    url.setPassword(ui->passwordLineEdit->text());
    return url;
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

    foreach (const QnConnectionData &connection, connections) {
        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName());
        m_connectionsModel->appendRow(row);
    }

    ui->connectionsComboBox->setCurrentIndex(0); /* At last used connection. */
    ui->passwordLineEdit->clear();
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

    if (!compatibilityChecker->isCompatible(QLatin1String("Client"), qApp->applicationVersion(), QLatin1String("ECS"), connectInfo->version)) {
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
    m_dataWidgetMapper->setCurrentModelIndex(m_connectionsModel->index(index, 0));
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
        QMessageBox::warning(this, tr("Invalid parameters"), tr("The information you have entered is not valid."));
        return;
    }

    QnConnectionDataList connections = qnSettings->customConnections();

    bool ok = false;

    QString defaultName(ui->connectionsComboBox->itemText(ui->connectionsComboBox->currentIndex()));
    if (defaultName == QnConnectionDataList::defaultLastUsedName()){
        defaultName = tr("%1 at %2").arg(ui->loginLineEdit->text()).arg(ui->hostnameLineEdit->text());
        if (connections.contains(defaultName))
            defaultName = connections.generateUniqueName(defaultName);
    }
    QString name = QInputDialog::getText(this, tr("Save connection as..."), tr("Enter name:"), QLineEdit::Normal, defaultName, &ok);
    if (!ok)
        return;

    QString password = ui->passwordLineEdit->text();

    if (connections.contains(name)){
       if (QMessageBox::warning(this, tr("Connection already exists"),
                                      tr("Connection with the same name already exists. Overwrite it?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::No){
           name = connections.generateUniqueName(name);
       } else {
           connections.removeOne(name);
       }
    }

    QnConnectionData connectionData(name, currentUrl());
    connectionData.url.setPassword(QString());
    connections.prepend(connectionData);
    qnSettings->setCustomConnections(connections);

    resetConnectionsModel();

    ui->passwordLineEdit->setText(password);

}

void LoginDialog::at_deleteButton_clicked() {
    QnConnectionDataList connections = qnSettings->customConnections();
    QString name = ui->connectionsComboBox->itemText(ui->connectionsComboBox->currentIndex());

    if (QMessageBox::warning(this, tr("Delete connections"),
                                   tr("Are you sure you want to delete the connection\n%1?").arg(name),
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;

    connections.removeOne(name);
    qnSettings->setCustomConnections(connections);
    resetConnectionsModel();
}
