#include "connections_settings_widget.h"
#include "ui_connections_settings_widget.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QStandardItemModel>
#include <QtGui/QMessageBox>

#include "api/AppServerConnection.h"

#include "ui/style/skin.h"
#include "ui/dialogs/connection_testing_dialog.h"

ConnectionsSettingsWidget::ConnectionsSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectionsSettingsWidget)
{
    ui->setupUi(this);

    // ### we do not have any advanced options for now
    ui->parametersTabWidget->removeTab(1);

    // ### default connection behavior is not implemented yet
    ui->defaultConnectionCheckBox->hide();

    // ### `save password` feature is not used
    ui->savePasswordCheckBox->hide();

    m_connectionsModel = new QStandardItemModel(this);
    m_connectionsRootItem = new QStandardItem(qnSkin->icon("folder.png"), tr("Connections"));
    m_connectionsModel->appendRow(m_connectionsRootItem);

    ui->connectionsTreeView->setModel(m_connectionsModel);
    ui->connectionsTreeView->setRootIsDecorated(false);
    ui->connectionsTreeView->expandAll();

    connect(ui->connectionsTreeView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentRowChanged(QModelIndex,QModelIndex)));

    m_dataWidgetMapper = new QDataWidgetMapper(this);
    m_dataWidgetMapper->setModel(m_connectionsModel);
    m_dataWidgetMapper->setRootIndex(m_connectionsRootItem->index());
    m_dataWidgetMapper->setSubmitPolicy(QDataWidgetMapper::AutoSubmit);
    m_dataWidgetMapper->setOrientation(Qt::Horizontal);
    m_dataWidgetMapper->addMapping(ui->connectionNameLineEdit, 0);
    m_dataWidgetMapper->addMapping(ui->hostnameLineEdit, 1);
    m_dataWidgetMapper->addMapping(ui->portLineEdit, 2);
    m_dataWidgetMapper->addMapping(ui->usernameLineEdit, 3);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 4);

    connect(ui->savePasswordCheckBox, SIGNAL(toggled(bool)), this, SLOT(savePasswordToggled(bool)));

    connect(ui->newConnectionButton, SIGNAL(clicked()), this, SLOT(newConnection()));
    connect(ui->testConnectionButton, SIGNAL(clicked()), this, SLOT(testConnection()));
    connect(ui->duplicateConnectionButton, SIGNAL(clicked()), this, SLOT(duplicateConnection()));
    connect(ui->deleteConnectionButton, SIGNAL(clicked()), this, SLOT(deleteConnection()));

    ui->connectionsTreeView->setCurrentIndex(m_connectionsRootItem->index());
    ui->connectionsTreeView->setFocus();
}

ConnectionsSettingsWidget::~ConnectionsSettingsWidget()
{
}

void ConnectionsSettingsWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        retranslateUi();
        break;
    default:
        break;
    }
}

void ConnectionsSettingsWidget::retranslateUi()
{
    m_connectionsRootItem->setText(tr("Connections"));
}

QList<QnSettings::ConnectionData> ConnectionsSettingsWidget::connections() const
{
    QList<QnSettings::ConnectionData> connections;
    
    for (int row = 0; row < m_connectionsRootItem->rowCount(); ++row)
    {
        QnSettings::ConnectionData connection;
        connection.name = m_connectionsRootItem->child(row, 0)->text();

        QString host = m_connectionsRootItem->child(row, 1)->text();
        int port = m_connectionsRootItem->child(row, 2)->text().toInt();
        
        connection.url.setScheme("http");
        connection.url.setHost(host);
        connection.url.setPort(port);
        connection.url.setUserName(m_connectionsRootItem->child(row, 3)->text());
        connection.url.setPassword(m_connectionsRootItem->child(row, 4)->text());

        if (connection.name.trimmed().isEmpty())
            connection.name = tr("Unnamed");

        connections.append(connection);
    }

    return connections;
}

void ConnectionsSettingsWidget::setConnections(const QList<QnSettings::ConnectionData> &connections)
{
    m_connectionsRootItem->removeRows(0, m_connectionsRootItem->rowCount());

    foreach (const QnSettings::ConnectionData &connection, connections)
        addConnection(connection);
}

void ConnectionsSettingsWidget::addConnection(const QnSettings::ConnectionData &connection)
{
    QList<QStandardItem *> row;
    row << new QStandardItem(!connection.name.trimmed().isEmpty() ? connection.name : tr("Unnamed"))
        << new QStandardItem(connection.url.host())
        << new QStandardItem(QString::number(connection.url.port()))
        << new QStandardItem(connection.url.userName())
        << new QStandardItem(connection.url.password())
        << new QStandardItem(connection.readOnly ? "ro" : "rw");
    m_connectionsRootItem->appendRow(row);
}

void ConnectionsSettingsWidget::removeConnection(const QnSettings::ConnectionData &connection)
{
    for (int row = 0; row < m_connectionsRootItem->rowCount(); ++row) {
        if (connection.name == m_connectionsRootItem->child(row, 0)->text()) {
            m_connectionsRootItem->removeRow(row);
            break;
        }
    }
}

void ConnectionsSettingsWidget::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    m_dataWidgetMapper->setCurrentModelIndex(current);

    const bool isOnRootItem = current == m_connectionsRootItem->index();
    bool disable = isOnRootItem;

    if (!isOnRootItem)
    {
        QModelIndex index = current.sibling(current.row(), 5);
        QStandardItem *item = m_connectionsModel->itemFromIndex(index);
        disable = (item->text() == "ro");
    }

    ui->connectionNameLineEdit->setEnabled(!disable);
    ui->hostnameLineEdit->setEnabled(!disable);
    ui->portLineEdit->setEnabled(!disable);
    ui->usernameLineEdit->setEnabled(!disable);
    ui->passwordLineEdit->setEnabled(!disable);
    ui->defaultConnectionCheckBox->setEnabled(!disable);

    ui->duplicateConnectionButton->setEnabled(!isOnRootItem);
    ui->testConnectionButton->setEnabled(!isOnRootItem);
    ui->deleteConnectionButton->setEnabled(!disable);

    if (previous != m_connectionsRootItem->index()) {
        QModelIndex index = previous.sibling(previous.row(), 0);
        QStandardItem *item = m_connectionsModel->itemFromIndex(index);
        if (item && item->text().trimmed().isEmpty())
            item->setText(tr("Unnamed"));
    }
}

QUrl ConnectionsSettingsWidget::currentUrl()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();

    QUrl url;
    
    QString host = m_connectionsModel->itemFromIndex(current.sibling(current.row(), 1))->text();
    int port = m_connectionsModel->itemFromIndex(current.sibling(current.row(), 2))->text().toInt();

    url.setScheme("http");
    url.setHost(host);
    url.setPort(port);
    url.setUserName(m_connectionsModel->itemFromIndex(current.sibling(current.row(), 3))->text());
    url.setPassword(m_connectionsModel->itemFromIndex(current.sibling(current.row(), 4))->text());

    return url;
}

void ConnectionsSettingsWidget::savePasswordToggled(bool save)
{
    if (!save)
        ui->passwordLineEdit->clear();
    ui->passwordLineEdit->setEnabled(save);
}

void ConnectionsSettingsWidget::testConnection()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();
    if (current == m_connectionsRootItem->index())
        return;

    QUrl url = currentUrl();
    
    if (!url.isValid())
    {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("The information you have entered is not valid."));
        return;
    }

    QScopedPointer<QnConnectionTestingDialog> dialog(new QnConnectionTestingDialog(url, this));
    dialog->setModal(true);
    dialog->exec();
}

void ConnectionsSettingsWidget::newConnection()
{
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("Unnamed"))
        << new QStandardItem()
        << new QStandardItem("8000")
        << new QStandardItem()
        << new QStandardItem()
        << new QStandardItem();
    m_connectionsRootItem->appendRow(row);

    ui->connectionsTreeView->setCurrentIndex(row.first()->index());

    ui->connectionNameLineEdit->setFocus();
    ui->connectionNameLineEdit->selectAll();
}

void ConnectionsSettingsWidget::duplicateConnection()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();
    if (current == m_connectionsRootItem->index())
        return;

    QList<QStandardItem *> row;
    for (int column = 0, columnCount = m_connectionsModel->columnCount(current.parent());
         column < columnCount; ++column) {
        QModelIndex index = current.sibling(current.row(), column);
        row << m_connectionsModel->itemFromIndex(index)->clone();
    }
    row.first()->setText(tr("Another ") + row.first()->text());
    row.at(5)->setText("rw");
    m_connectionsRootItem->appendRow(row);

    ui->connectionsTreeView->setCurrentIndex(row.first()->index());

    ui->connectionNameLineEdit->setFocus();
    ui->connectionNameLineEdit->selectAll();
}

void ConnectionsSettingsWidget::deleteConnection()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();
    if (current == m_connectionsRootItem->index())
        return;

    m_connectionsModel->removeRow(current.row(), current.parent());
}
