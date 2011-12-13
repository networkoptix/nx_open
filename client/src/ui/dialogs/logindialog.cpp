#include "logindialog.h"
#include "ui_logindialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "ui/preferences/preferences_wnd.h"
#include "ui/skin.h"

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
    m_dataWidgetMapper->addMapping(ui->usernameLineEdit, 2);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 3);

    connect(ui->configureStoredConnectionsButton, SIGNAL(clicked()), this, SLOT(configureStoredConnections()));

    ui->buttonBox->button(QDialogButtonBox::Reset)->setText(tr("&Reset"));

    if (QPushButton *button = ui->buttonBox->button(QDialogButtonBox::Reset))
        connect(button, SIGNAL(clicked()), this, SLOT(reset()));

    updateStoredConnections();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::accept()
{
    const int row = ui->storedConnectionsComboBox->currentIndex();

    Settings::ConnectionData connection;
    // connection.name = m_connectionsModel->item(row, 0)->text();
    QString str = m_connectionsModel->item(row, 1)->text();
    if (!str.isEmpty()) {
        const int idx = str.indexOf(QLatin1String("://"));
        if (idx == -1)
            str.prepend(QLatin1String("http://"));
        else if (str.left(idx) != QLatin1String("http"))
            str.replace(0, idx, QLatin1String("http"));
        connection.url.setUrl(str, QUrl::StrictMode);
        if (connection.url.isValid()) {
            connection.url.setUserName(m_connectionsModel->item(row, 2)->text());
            connection.url.setPassword(m_connectionsModel->item(row, 3)->text());
        }
    }

    if (!connection.url.isValid()) {
        QMessageBox::warning(this, tr("Invalid Login Information"), tr("The Login Information you have entered is not valid."));
        return;
    }

    Settings::setLastUsedConnection(connection);

    QDialog::accept();
}

void LoginDialog::reset()
{
    ui->hostnameLineEdit->clear();
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
    Settings::ConnectionData lastUsedConnection = Settings::lastUsedConnection();
    foreach (const Settings::ConnectionData &connection, Settings::connections()) {
        if (connection.name.trimmed().isEmpty() || !connection.url.isValid())
            continue;

        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.toString(QUrl::RemoveUserInfo))
            << new QStandardItem(connection.url.userName())
            << new QStandardItem(connection.url.password());
        m_connectionsModel->appendRow(row);

        if (lastUsedConnection == connection)
            index = m_connectionsModel->rowCount() - 1;
    }
    {
        Settings::ConnectionData connection;
        if (index == -1 || lastUsedConnection.name.trimmed().isEmpty()) {
            connection = lastUsedConnection;
            if (index == -1) {
                index = 0;
                connection.name.clear();
            }
        }
        QList<QStandardItem *> row;
        row << new QStandardItem(connection.name)
            << new QStandardItem(connection.url.toString(QUrl::RemoveUserInfo))
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
    PreferencesWindow preferencesDialog(this);
    preferencesDialog.setCurrentTab(2);

    if (preferencesDialog.exec() == QDialog::Accepted)
        updateStoredConnections();
}
