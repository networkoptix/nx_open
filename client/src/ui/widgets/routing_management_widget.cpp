#include "routing_management_widget.h"
#include "ui_routing_management_widget.h"

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

#include "api/app_server_connection.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "core/resource/media_server_resource.h"
#include "ui/models/resource_pool_model.h"
#include "ui/models/server_addresses_model.h"

namespace {
    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
}

QnRoutingManagementWidget::QnRoutingManagementWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnRoutingManagementWidget)
{
    ui->setupUi(this);

    QnResourcePoolModel *serversModel = new QnResourcePoolModel(Qn::ServersNode, this);
    ui->serversView->setModel(serversModel);

    m_serverAddressesModel = new QnServerAddressesModel(this);
    m_sortedServerAddressesModel = new QnSortedServerAddressesModel(this);
    m_sortedServerAddressesModel->setSourceModel(m_serverAddressesModel);
    ui->addressesView->setModel(m_sortedServerAddressesModel);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QnServerAddressesModel::AddressColumn, QHeaderView::Stretch);
    ui->addressesView->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->serversView->selectionModel(),  &QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::at_serversView_currentIndexChanged);
    connect(ui->addressesView->selectionModel(),&QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::at_addressesView_currentIndexChanged);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::ignoreChangeRequested, this,   &QnRoutingManagementWidget::at_serverAddressesModel_ignoreChangeRequested);
    connect(ui->addButton,                      &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_addButton_clicked);
    connect(ui->removeButton,                   &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_removeButton_clicked);
}

QnRoutingManagementWidget::~QnRoutingManagementWidget() {}

QnMediaServerResourcePtr QnRoutingManagementWidget::currentServer() const {
    return ui->serversView->currentIndex().data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
}

void QnRoutingManagementWidget::updateModel(const QnMediaServerResourcePtr &server) {
    QStringList addresses;
    foreach (const QHostAddress &address, server->getNetAddrList())
        addresses.append(address.toString());

    QStringList manualAddresses;
    foreach (const QUrl &url, server->getAdditionalUrls())
        manualAddresses.append(url.toString());

    QSet<QString> ignoredAddresses;
    foreach (const QUrl &url, server->getIgnoredUrls())
        ignoredAddresses.insert(url.host());


    int row = ui->addressesView->currentIndex().row();

    m_serverAddressesModel->resetModel(addresses, manualAddresses, ignoredAddresses);

    if (row < m_sortedServerAddressesModel->rowCount())
        ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));
}

void QnRoutingManagementWidget::at_addButton_clicked() {
    QnMediaServerResourcePtr server = currentServer();
    if (!server)
        return;

    QString urlString = QInputDialog::getText(this, tr("Enter URL"), tr("URL"));
    if (urlString.isEmpty())
        return;

    QUrl url = QUrl::fromUserInput(urlString);
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid URL."));
        return;
    }

    QList<QUrl> urls = server->getAdditionalUrls();
    if (urls.contains(url))
        return;

    urls.append(url);

    connection2()->getDiscoveryManager()->addDiscoveryInformation(server->getId(), QList<QUrl>() << url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    server->setAdditionalUrls(urls);
}

void QnRoutingManagementWidget::at_removeButton_clicked() {
    QModelIndex index = m_sortedServerAddressesModel->mapToSource(ui->addressesView->currentIndex());
    if (!index.isValid())
        return;

    QnMediaServerResourcePtr server = currentServer();
    if (!server)
        return;

    QUrl url = QUrl(index.data().toString());
    QList<QUrl> urls = server->getAdditionalUrls();
    if (!urls.removeOne(url))
        return;

    connection2()->getDiscoveryManager()->removeDiscoveryInformation(server->getId(), QList<QUrl>() << url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    server->setAdditionalUrls(urls);
}

void QnRoutingManagementWidget::at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous) {
    QnMediaServerResourcePtr oldServer = previous.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
    if (oldServer)
        oldServer->disconnect(this);

    QnMediaServerResourcePtr server = current.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
    if (!server) {
        m_serverAddressesModel->resetModel(QStringList(), QStringList(), QSet<QString>());
        return;
    }

    updateModel(server);

    connect(server.data(),      &QnMediaServerResource::resourceChanged,    this,   &QnRoutingManagementWidget::at_currentServer_changed);
    connect(server.data(),      &QnMediaServerResource::auxUrlsChanged,     this,   &QnRoutingManagementWidget::at_currentServer_changed);
}

void QnRoutingManagementWidget::at_addressesView_currentIndexChanged(const QModelIndex &index) {
    QModelIndex sourceIndex = m_sortedServerAddressesModel->mapToSource(index);

    bool manual = m_serverAddressesModel->isManualAddress(sourceIndex);

    ui->removeButton->setEnabled(manual);
}

void QnRoutingManagementWidget::at_currentServer_changed(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();
    updateModel(server);
}

void QnRoutingManagementWidget::at_serverAddressesModel_ignoreChangeRequested(const QString &address, bool ignore) {
    QnMediaServerResourcePtr server = currentServer();
    if (!server)
        return;

    if (!server->getNetAddrList().contains(QHostAddress(address)))
        return;

    QUrl url = QUrl(QString(lit("http://%1")).arg(address));

    QList<QUrl> ignoredUrls = server->getIgnoredUrls();

    if (ignore) {
        if (ignoredUrls.contains(url))
            return;
        ignoredUrls.append(url);
        connection2()->getDiscoveryManager()->addDiscoveryInformation(server->getId(), QList<QUrl>() << url, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    } else {
        if (!ignoredUrls.removeOne(url))
            return;
        connection2()->getDiscoveryManager()->removeDiscoveryInformation(server->getId(), QList<QUrl>() << url, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    server->setIgnoredUrls(ignoredUrls);
}
