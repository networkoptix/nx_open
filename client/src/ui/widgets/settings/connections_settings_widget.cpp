#include "connections_settings_widget.h"
#include "ui_connections_settings_widget.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QStandardItemModel>
#include <QtGui/QMessageBox>

#include "api/app_server_connection.h"

#include "ui/style/skin.h"
#include "ui/dialogs/connection_testing_dialog.h"

QnConnectionsSettingsWidget::QnConnectionsSettingsWidget(QWidget *parent) :
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
    m_connectionsRootItem = new QStandardItem(qnSkin->icon("folder.png"), QString());
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


    retranslateUi();
}

QnConnectionsSettingsWidget::~QnConnectionsSettingsWidget()
{
}

void QnConnectionsSettingsWidget::changeEvent(QEvent *event)
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

void QnConnectionsSettingsWidget::retranslateUi()
{
    m_connectionsRootItem->setText(tr("Connections"));
}

QnConnectionDataList QnConnectionsSettingsWidget::connections() const
{
    QnConnectionDataList connections;
    
    for (int row = 0; row < m_connectionsRootItem->rowCount(); ++row)
    {
        QnConnectionData connection;
        connection.name = m_connectionsRootItem->child(row, 0)->text();

        QString host = m_connectionsRootItem->child(row, 1)->text();
        int port = m_connectionsRootItem->child(row, 2)->text().toInt();
        
        connection.url.setScheme(QLatin1String("https"));
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

void QnConnectionsSettingsWidget::setConnections(const QnConnectionDataList &connections)
{
    m_connectionsRootItem->removeRows(0, m_connectionsRootItem->rowCount());

    foreach (const QnConnectionData &connection, connections)
        addConnection(connection);
}

void QnConnectionsSettingsWidget::addConnection(const QnConnectionData &connection)
{
    QList<QStandardItem *> row;
    row << new QStandardItem(!connection.name.trimmed().isEmpty() ? connection.name : tr("Unnamed"))
        << new QStandardItem(connection.url.host())
        << new QStandardItem(QString::number(connection.url.port()))
        << new QStandardItem(connection.url.userName())
        << new QStandardItem(connection.url.password())
        << new QStandardItem(connection.readOnly ? QLatin1String("ro") : QLatin1String("rw"));
    m_connectionsRootItem->appendRow(row);
}

void QnConnectionsSettingsWidget::removeConnection(const QnConnectionData &connection)
{
    for (int row = 0; row < m_connectionsRootItem->rowCount(); ++row) {
        if (connection.name == m_connectionsRootItem->child(row, 0)->text()) {
            m_connectionsRootItem->removeRow(row);
            break;
        }
    }
}

void QnConnectionsSettingsWidget::currentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    m_dataWidgetMapper->setCurrentModelIndex(current);

    const bool isOnRootItem = current == m_connectionsRootItem->index();
    bool disable = isOnRootItem;

    if (!isOnRootItem)
    {
        QModelIndex index = current.sibling(current.row(), 5);
        QStandardItem *item = m_connectionsModel->itemFromIndex(index);
        disable = (item->text() == QLatin1String("ro"));
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

QUrl QnConnectionsSettingsWidget::currentUrl()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();

    QUrl url;
    
    QString host = m_connectionsModel->itemFromIndex(current.sibling(current.row(), 1))->text();
    int port = m_connectionsModel->itemFromIndex(current.sibling(current.row(), 2))->text().toInt();

    url.setScheme(QLatin1String("https"));
    url.setHost(host);
    url.setPort(port);
    url.setUserName(m_connectionsModel->itemFromIndex(current.sibling(current.row(), 3))->text());
    url.setPassword(m_connectionsModel->itemFromIndex(current.sibling(current.row(), 4))->text());

    return url;
}

void QnConnectionsSettingsWidget::savePasswordToggled(bool save)
{
    if (!save)
        ui->passwordLineEdit->clear();
    ui->passwordLineEdit->setEnabled(save);
}

void QnConnectionsSettingsWidget::testConnection()
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

void QnConnectionsSettingsWidget::newConnection()
{
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("Unnamed"))
        << new QStandardItem()
        << new QStandardItem(QLatin1String("8000"))
        << new QStandardItem()
        << new QStandardItem()
        << new QStandardItem();
    m_connectionsRootItem->appendRow(row);

    ui->connectionsTreeView->setCurrentIndex(row.first()->index());

    ui->connectionNameLineEdit->setFocus();
    ui->connectionNameLineEdit->selectAll();
}

void QnConnectionsSettingsWidget::duplicateConnection()
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
    row.at(5)->setText(QLatin1String("rw"));
    m_connectionsRootItem->appendRow(row);

    ui->connectionsTreeView->setCurrentIndex(row.first()->index());

    ui->connectionNameLineEdit->setFocus();
    ui->connectionNameLineEdit->selectAll();
}

void QnConnectionsSettingsWidget::deleteConnection()
{
    const QModelIndex current = ui->connectionsTreeView->currentIndex();
    if (current == m_connectionsRootItem->index())
        return;

    m_connectionsModel->removeRow(current.row(), current.parent());
}
