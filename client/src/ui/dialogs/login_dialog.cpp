#include "login_dialog.h"
#include "ui_login_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/workbench/workbench_context.h"
#include "connection_testing_dialog.h"

#include "api/AppServerConnection.h"

#include "settings.h"

LoginDialog::LoginDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog),
    m_context(context)
{
    if(!context)
        qnNullWarning(context);

    ui->setupUi(this);
    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);

    /* Don't allow to save passwords, at least for now. */
    ui->savePasswordCheckBox->hide();

    ui->titleLabel->setAlignment(Qt::AlignCenter);
    ui->titleLabel->setPixmap(Skin::pixmap(QLatin1String("logo_1920_1080.png")).scaledToWidth(250)); // ###

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
}

LoginDialog::~LoginDialog()
{
}

QUrl LoginDialog::currentUrl()
{
    const int row = ui->connectionsComboBox->currentIndex();

    QUrl url;

    QString host = m_connectionsModel->item(row, 1)->text();
    int port = m_connectionsModel->item(row, 2)->text().toInt();

    url.setScheme("http");
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

    QnSettings::ConnectionData connection;
    connection.url = currentUrl();

    if (!connection.url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    qnSettings->setLastUsedConnection(connection);

    QDialog::accept();
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

    int index = -1;
    QnSettings::ConnectionData lastUsedConnection = qnSettings->lastUsedConnection();
    foreach (const QnSettings::ConnectionData &connection, qnSettings->connections()) {
        if (connection.name.trimmed().isEmpty() || !connection.url.isValid())
            continue;

        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->appendRow(row);

        if (lastUsedConnection == connection)
            index = m_connectionsModel->rowCount() - 1;
    }
    {
        QnSettings::ConnectionData connection;
        if (index == -1 || lastUsedConnection.name.trimmed().isEmpty()) {
            connection = lastUsedConnection;
            if (index == -1) {
                index = 0;
                connection.name.clear();
            }
        }
        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.host())
            << new QStandardItem(QString::number(connection.url.port()))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->insertRow(0, row);
    }

    ui->connectionsComboBox->setCurrentIndex(index);
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
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

    ConnectionTestingDialog dialog(this, url);
    dialog.setModal(true);
    dialog.exec();
}

void LoginDialog::at_configureConnectionsButton_clicked()
{
    QScopedPointer<PreferencesDialog> dialog(new PreferencesDialog(m_context.data(), this));
    dialog->setCurrentPage(PreferencesDialog::PageConnections);

    if (dialog->exec() == QDialog::Accepted)
        updateStoredConnections();
}
