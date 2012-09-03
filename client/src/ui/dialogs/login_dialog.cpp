#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtCore/QEvent>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
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
#include "plugins/resources/archive/filetypes.h"
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
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 4);

    resetConnectionsModel();
    updateFocus();
}

LoginDialog::~LoginDialog() {
    return;
}

void LoginDialog::updateFocus() 
{
    int size = m_dataWidgetMapper->model()->columnCount();

    QWidget *widget = NULL;
    for(int i = 0; i < size; i++) {
        widget = m_dataWidgetMapper->mappedWidgetAt(i);
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
    
    /* Set focus on the last widget in list if every widget is filled. */
    if(widget)
        widget->setFocus();
}

QUrl LoginDialog::currentUrl() const {
    const int row = ui->connectionsComboBox->currentIndex();

    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(ui->hostnameLineEdit->text());
    url.setPort(ui->portSpinBox->value());
    url.setUserName(ui->loginLineEdit->text());
    url.setPassword(ui->passwordLineEdit->text());

    /*url.setHost(m_connectionsModel->item(row, 1)->text());
    url.setPort(m_connectionsModel->item(row, 2)->text().toInt());
    url.setUserName(m_connectionsModel->item(row, 3)->text());
    url.setPassword(m_connectionsModel->item(row, 4)->text());
*/
    return url;
}

QnConnectInfoPtr LoginDialog::currentInfo() const {
    return m_connectInfo;
}

void LoginDialog::accept() {
    /* Widget data may not be written out to the model yet. Force it. */
 //   m_dataWidgetMapper->submit();

    QUrl url = currentUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection(url);
    m_requestHandle = connection->connectAsync(this, SLOT(at_connectFinished(int, const QByteArray &, QnConnectInfoPtr, int)));

    {
        // TODO: #gdm ask Elrik about it a bit later
        // Temporary 1.0/1.1 version check.
        // Let's remove it 1.3/1.4.
        QUrl httpUrl;
        httpUrl.setHost(url.host());
        httpUrl.setPort(url.port());
        httpUrl.setScheme(QLatin1String("http"));
        httpUrl.setUserName(QString());
        httpUrl.setPassword(QString());
        QnSessionManager::instance()->sendAsyncGetRequest(httpUrl, QLatin1String("resourceEx"), this, SLOT(at_oldHttpConnectFinished(int,QByteArray,QByteArray,int)));
    }

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
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->appendRow(row);
    }

    ui->connectionsComboBox->setCurrentIndex(0); /* At last used connection. */
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
void LoginDialog::at_oldHttpConnectFinished(int status, QByteArray /*errorString*/, QByteArray /*data*/, int /*handle*/) {
    if (status == 204) {
        m_requestHandle = -1;

        updateUsability();

        QMessageBox::warning(
            this,
            tr("Could not connect to Enterprise Controller"),
            tr("Connection could not be established.\nThe Enterprise Controller is incompatible. Please upgrade your enterprise controller or contact VMS administrator.")
        );
    }
}

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
