#include "logindialog.h"
#include "ui_logindialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "connectiontestingdialog.h"

#include "api/AppServerConnection.h"

#include "settings.h"

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // ### `don't save password` feature is not implemented yet
    ui->savePasswordCheckBox->hide();

    ui->titleLabel->setAlignment(Qt::AlignCenter);
    ui->titleLabel->setPixmap(Skin::pixmap(QLatin1String("logo_1920_1080.png")).scaledToWidth(250)); // ###

    m_connectionsModel = new QStandardItemModel(this);

    ui->storedConnectionsComboBox->setModel(m_connectionsModel);

    connect(ui->storedConnectionsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(currentIndexChanged(int)));

    m_dataWidgetMapper = new QDataWidgetMapper(this);
    m_dataWidgetMapper->setModel(m_connectionsModel);
    m_dataWidgetMapper->setSubmitPolicy(QDataWidgetMapper::AutoSubmit);
    m_dataWidgetMapper->setOrientation(Qt::Horizontal);
    m_dataWidgetMapper->addMapping(ui->hostnameLineEdit, 1);
    m_dataWidgetMapper->addMapping(ui->portLineEdit, 2);
    m_dataWidgetMapper->addMapping(ui->usernameLineEdit, 3);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 4);

    connect(ui->configureStoredConnectionsButton, SIGNAL(clicked()), this, SLOT(configureStoredConnections()));

    ui->buttonBox->button(QDialogButtonBox::Reset)->setText(tr("&Reset"));

    if (QPushButton *button = ui->buttonBox->button(QDialogButtonBox::Reset))
        connect(button, SIGNAL(clicked()), this, SLOT(reset()));

    updateStoredConnections();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::testSettings()
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

QUrl LoginDialog::currentUrl()
{
    const int row = ui->storedConnectionsComboBox->currentIndex();

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
    ui->portLineEdit->clear();
    ui->usernameLineEdit->clear();
    ui->passwordLineEdit->clear();

    updateStoredConnections();
}

void LoginDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        //retranslateUi();
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

    ui->storedConnectionsComboBox->setCurrentIndex(index);
}

void LoginDialog::currentIndexChanged(int index)
{
    m_dataWidgetMapper->setCurrentModelIndex(m_connectionsModel->index(index, 0));
}

void LoginDialog::configureStoredConnections()
{
    PreferencesDialog dialog(this);
    dialog.setCurrentPage(PreferencesDialog::PageConnections);

    if (dialog.exec() == QDialog::Accepted)
        updateStoredConnections();
}
