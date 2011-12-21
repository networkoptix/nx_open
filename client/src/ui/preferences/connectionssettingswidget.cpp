#include "connectionssettingswidget.h"
#include "ui_connectionssettingswidget.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QStandardItemModel>

#include "ui/skin/skin.h"

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
    m_connectionsRootItem = new QStandardItem(Skin::icon(QLatin1String("folder.png")), tr("Connections"));
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
    m_dataWidgetMapper->addMapping(ui->usernameLineEdit, 2);
    m_dataWidgetMapper->addMapping(ui->passwordLineEdit, 3);

    connect(ui->savePasswordCheckBox, SIGNAL(toggled(bool)), this, SLOT(savePasswordToggled(bool)));

    connect(ui->newConnectionButton, SIGNAL(clicked()), this, SLOT(newConnection()));
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

QList<ConnectionsSettingsWidget::ConnectionData> ConnectionsSettingsWidget::connections() const
{
    QList<ConnectionData> connections;
    for (int row = 0; row < m_connectionsRootItem->rowCount(); ++row) {
        ConnectionData connection;
        connection.name = m_connectionsRootItem->child(row, 0)->text();
        QString str = m_connectionsRootItem->child(row, 1)->text();
        if (!str.isEmpty()) {
            const int idx = str.indexOf(QLatin1String("://"));
            if (idx == -1)
                str.prepend(QLatin1String("http://"));
            else if (str.left(idx) != QLatin1String("http"))
                str.replace(0, idx, QLatin1String("http"));
            connection.url.setUrl(str, QUrl::StrictMode);
            /*if (connection.url.isValid()) */{
                connection.url.setUserName(m_connectionsRootItem->child(row, 2)->text());
                connection.url.setPassword(m_connectionsRootItem->child(row, 3)->text());
            }
        }
        if (connection.name.trimmed().isEmpty())
            connection.name = tr("Unnamed");

        connections.append(connection);
    }

    return connections;
}

void ConnectionsSettingsWidget::setConnections(const QList<ConnectionsSettingsWidget::ConnectionData> &connections)
{
    m_connectionsRootItem->removeRows(0, m_connectionsRootItem->rowCount());

    foreach (const ConnectionData &connection, connections)
        addConnection(connection);
}

void ConnectionsSettingsWidget::addConnection(const ConnectionsSettingsWidget::ConnectionData &connection)
{
    QList<QStandardItem *> row;
    row << new QStandardItem(!connection.name.trimmed().isEmpty() ? connection.name : tr("Unnamed"))
        << new QStandardItem(connection.url.toString(QUrl::RemoveUserInfo))
        << new QStandardItem(connection.url.userName())
        << new QStandardItem(connection.url.password());
    m_connectionsRootItem->appendRow(row);
}

void ConnectionsSettingsWidget::removeConnection(const ConnectionsSettingsWidget::ConnectionData &connection)
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
    ui->connectionNameLineEdit->setEnabled(!isOnRootItem);
    ui->hostnameLineEdit->setEnabled(!isOnRootItem);
    ui->usernameLineEdit->setEnabled(!isOnRootItem);
    ui->passwordLineEdit->setEnabled(!isOnRootItem);
    ui->defaultConnectionCheckBox->setEnabled(!isOnRootItem);

    ui->duplicateConnectionButton->setEnabled(!isOnRootItem);
    ui->deleteConnectionButton->setEnabled(!isOnRootItem);

    if (previous != m_connectionsRootItem->index()) {
        QModelIndex index = previous.sibling(previous.row(), 0);
        QStandardItem *item = m_connectionsModel->itemFromIndex(index);
        if (item && item->text().trimmed().isEmpty())
            item->setText(tr("Unnamed"));
    }
}

void ConnectionsSettingsWidget::savePasswordToggled(bool save)
{
    if (!save)
        ui->passwordLineEdit->clear();
    ui->passwordLineEdit->setEnabled(save);
}

void ConnectionsSettingsWidget::newConnection()
{
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("Unnamed"))
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
