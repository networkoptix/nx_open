#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <utils/settings.h>
#include <core/resource/resource.h>
#include <api/AppServerConnection.h>
#include <api/SessionManager.h>

#include <ui/dialogs/preferences_dialog.h>
#include <ui/widgets/rendering_widget.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "plugins/resources/archive/filetypes.h"
#include "plugins/resources/archive/filetypesupport.h"

#include "connection_testing_dialog.h"

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
    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);

    /* Don't allow to save passwords, at least for now. */
    //ui->savePasswordCheckBox->hide();

    QDir dir(":/skin");
    QStringList	introList = dir.entryList(QStringList() << "intro.*");
    QString resourceName = ":/skin/intro";
    if (!introList.isEmpty())
        resourceName = QString(":/skin/") + introList.first();

    QnAviResourcePtr resource = QnAviResourcePtr(new QnAviResource(QString("qtfile://") + resourceName));
    if (FileTypeSupport::isImageFileExt(resourceName))
        resource->addFlags(QnResource::still_image);

    m_renderingWidget = new QnRenderingWidget();
    m_renderingWidget->setResource(resource);

    QVBoxLayout* layout = new QVBoxLayout(ui->videoSpacer);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,10);
    layout->addWidget(m_renderingWidget);

    resetButton->setText(tr("&Reset"));

    m_connectionsModel = new QStandardItemModel(this);
    ui->connectionsComboBox->setModel(m_connectionsModel);

    connect(ui->connectionsComboBox,        SIGNAL(currentIndexChanged(int)),       this,   SLOT(at_connectionsComboBox_currentIndexChanged(int)));
    connect(ui->testButton,                 SIGNAL(clicked()),                      this,   SLOT(at_testButton_clicked()));
    connect(ui->configureConnectionsButton, SIGNAL(clicked()),                      this,   SLOT(at_configureConnectionsButton_clicked()));
    connect(ui->passwordLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->loginLineEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->hostnameLineEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateAcceptibility()));
    connect(ui->portSpinBox,                SIGNAL(valueChanged(int)),              this,   SLOT(updateAcceptibility()));
    connect(ui->buttonBox,                  SIGNAL(accepted()),                     this,   SLOT(accept()));
    connect(ui->buttonBox,                  SIGNAL(rejected()),                     this,   SLOT(reject()));
    connect(resetButton,                    SIGNAL(clicked()),                      this,   SLOT(reset()));

    m_dataWidgetMapper = new QDataWidgetMapper(this);
    m_dataWidgetMapper->setModel(m_connectionsModel);
    m_dataWidgetMapper->setSubmitPolicy(QDataWidgetMapper::AutoSubmit);
    m_dataWidgetMapper->setOrientation(Qt::Horizontal);
    m_dataWidgetMapper->addMapping(ui->hostnameLineEdit, 1);
    m_dataWidgetMapper->addMapping(ui->portSpinBox, 2);
    m_dataWidgetMapper->addMapping(ui->loginLineEdit, 3);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 4);

    updateStoredConnections();
    updateFocus();
}

LoginDialog::~LoginDialog() {
    return;
}

void LoginDialog::updateFocus() 
{
    int size = m_dataWidgetMapper->model()->columnCount();

    int i;
    for(i = 0; i < size; i++) {
        QWidget *widget = m_dataWidgetMapper->mappedWidgetAt(i);
        if(!widget)
            continue;

        QByteArray propertyName = m_dataWidgetMapper->mappedPropertyName(widget);
        QVariant value = widget->property(propertyName.constData());
        if(!value.isValid())
            continue;

        if(value.toString().isEmpty())
            break;

        if((value.userType() == QVariant::Int || value.userType() == QVariant::LongLong) && value.toInt() == 0)
            break;
    }

    if(i < size)
        m_dataWidgetMapper->mappedWidgetAt(i)->setFocus();
}

QUrl LoginDialog::currentUrl()
{
    const int row = ui->connectionsComboBox->currentIndex();

    QUrl url;

    QString host = m_connectionsModel->item(row, 1)->text();
    int port = m_connectionsModel->item(row, 2)->text().toInt();

    url.setScheme("https");
    url.setHost(host);
    url.setPort(port);
    url.setUserName(m_connectionsModel->item(row, 3)->text());
    url.setPassword(m_connectionsModel->item(row, 4)->text());

    return url;
}

void LoginDialog::accept()
{
    /* Widget data may not be written out to the model yet. Force it. */
    m_dataWidgetMapper->submit();

    QUrl url = currentUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(url);
    m_requestHandle = connection->testConnectionAsync(this, SLOT(at_testFinished(int, const QByteArray &, const QByteArray &, int)));

    updateUsability();
}

void LoginDialog::reject() 
{
    if(m_requestHandle == -1) {
        QDialog::reject();
        return;
    }

    m_requestHandle = -1;
    updateUsability();
}

void LoginDialog::reset()
{
    ui->hostnameLineEdit->clear();
    ui->portSpinBox->setValue(0);
    ui->loginLineEdit->clear();
    ui->passwordLineEdit->clear();

    updateStoredConnections();
}

void LoginDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void LoginDialog::updateStoredConnections()
{
    m_connectionsModel->removeRows(0, m_connectionsModel->rowCount());

    QnConnectionDataList connections = qnSettings->customConnections();
    connections.push_front(qnSettings->defaultConnection());

    QnConnectionData lastUsedConnection = qnSettings->lastUsedConnection();
    if(!lastUsedConnection.url.isValid()) {
        lastUsedConnection = qnSettings->defaultConnection();
        lastUsedConnection.name = QString();
    }
    if(connections.contains(lastUsedConnection))
        connections.removeOne(lastUsedConnection);
    connections.push_front(lastUsedConnection);

    foreach (const QnConnectionData &connection, connections) {
        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->appendRow(row);
    }

    ui->connectionsComboBox->setCurrentIndex(0); /* At last used connection. */
}

void LoginDialog::updateAcceptibility() 
{
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
void LoginDialog::at_testFinished(int status, const QByteArray &/*data*/, const QByteArray &/*errorString*/, int requestHandle)
{
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
        return;
    }

    QnConnectionData connectionData;
    connectionData.url = currentUrl();
    qnSettings->setLastUsedConnection(connectionData);

    QDialog::accept();
}

void LoginDialog::at_connectionsComboBox_currentIndexChanged(int index)
{
    m_dataWidgetMapper->setCurrentModelIndex(m_connectionsModel->index(index, 0));
}

void LoginDialog::at_testButton_clicked()
{
    QUrl url = currentUrl();

    if (!url.isValid())
    {
        QMessageBox::warning(this, tr("Invalid paramters"), tr("The information you have entered is not valid."));
        return;
    }

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(url, this));
    dialog->setModal(true);
    dialog->exec();
}

void LoginDialog::at_configureConnectionsButton_clicked()
{
    QScopedPointer<QnPreferencesDialog> dialog(new QnPreferencesDialog(m_context.data(), this));
    dialog->setCurrentPage(QnPreferencesDialog::PageConnections);

    if (dialog->exec() == QDialog::Accepted)
        updateStoredConnections();
}
