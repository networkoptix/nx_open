#include "routing_management_widget.h"
#include "ui_routing_management_widget.h"

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

#include "api/app_server_connection.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "ui/models/resource_list_model.h"
#include "ui/models/server_addresses_model.h"
#include "ui/style/warning_style.h"
#include "common/common_module.h"
#include "utils/common/string.h"

namespace {
    const int defaultRtspPort = 7001;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }

    class SortedServersProxyModel : public QSortFilterProxyModel {
    public:
        SortedServersProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent) {}
    protected:
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
            QString leftString = left.data(sortRole()).toString();
            QString rightString = right.data(sortRole()).toString();
            return naturalStringLessThan(leftString, rightString);
        }
    };
}

QnRoutingManagementWidget::QnRoutingManagementWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnRoutingManagementWidget)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    m_serverListModel = new QnResourceListModel(this);
    SortedServersProxyModel *sortedServersModel = new SortedServersProxyModel(this);
    sortedServersModel->setSourceModel(m_serverListModel);
    sortedServersModel->setDynamicSortFilter(true);
    sortedServersModel->setSortRole(Qt::DisplayRole);
    sortedServersModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    sortedServersModel->sort(Qn::NameColumn);
    ui->serversView->setModel(sortedServersModel);

    m_serverAddressesModel = new QnServerAddressesModel(this);
    m_sortedServerAddressesModel = new QnSortedServerAddressesModel(this);
    m_sortedServerAddressesModel->setSourceModel(m_serverAddressesModel);
    ui->addressesView->setModel(m_sortedServerAddressesModel);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QnServerAddressesModel::AddressColumn, QHeaderView::Stretch);
    ui->addressesView->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->serversView->selectionModel(),  &QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::at_serversView_currentIndexChanged);
    connect(ui->addressesView->selectionModel(),&QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::updateUi);
    connect(ui->addressesView,                  &QAbstractItemView::doubleClicked,              this,   &QnRoutingManagementWidget::at_addressesView_doubleClicked);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::ignoreChangeRequested, this,   &QnRoutingManagementWidget::at_serverAddressesModel_ignoreChangeRequested);
    connect(ui->addButton,                      &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_addButton_clicked);
    connect(ui->removeButton,                   &QPushButton::clicked,                          this,   &QnRoutingManagementWidget::at_removeButton_clicked);

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnRoutingManagementWidget::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnRoutingManagementWidget::at_resourcePool_resourceRemoved);

    m_serverListModel->setResources(qnResPool->getResourcesWithFlag(Qn::server));

    updateUi();
}

QnRoutingManagementWidget::~QnRoutingManagementWidget() {}

void QnRoutingManagementWidget::updateFromSettings() {
    ui->warningLabel->hide();
}

bool QnRoutingManagementWidget::confirm() {
    ui->warningLabel->hide();
    return true;
}

void QnRoutingManagementWidget::updateModel() {
    if (!m_server) {
        m_serverAddressesModel->clear();
        return;
    }

    int port = QUrl(m_server->getApiUrl()).port();
    if (port == -1)
        port = defaultRtspPort;

    QList<QUrl> addresses;
    foreach (const QHostAddress &address, m_server->getNetAddrList()) {
        QUrl url;
        url.setScheme(lit("http"));
        url.setHost(address.toString());
        url.setPort(port);
        url.setPath(QString());

        addresses.append(url);
    }

    QList<QUrl> manualAddresses;
    foreach (const QUrl &url, m_server->getAdditionalUrls()) {
        QUrl actualUrl = url;
        if (actualUrl.port() == -1)
            actualUrl.setPort(port);
        manualAddresses.append(actualUrl);
    }

    QSet<QUrl> ignoredAddresses;
    foreach (const QUrl &url, m_server->getIgnoredUrls()) {
        QUrl actualUrl = url;
        if (actualUrl.port() == -1)
            actualUrl.setPort(port);
        ignoredAddresses.insert(actualUrl);
    }


    int row = ui->addressesView->currentIndex().row();

    m_serverAddressesModel->resetModel(addresses, manualAddresses, ignoredAddresses);

    if (row < m_sortedServerAddressesModel->rowCount())
        ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));
}

void QnRoutingManagementWidget::updateUi() {
    QModelIndex sourceIndex = m_sortedServerAddressesModel->mapToSource(ui->addressesView->currentIndex());

    ui->addButton->setEnabled(ui->serversView->currentIndex().isValid());
    ui->removeButton->setEnabled(m_serverAddressesModel->isManualAddress(sourceIndex));
}

void QnRoutingManagementWidget::at_addButton_clicked() {
    if (!m_server)
        return;

    QString urlString = QInputDialog::getText(this, tr("Enter URL"), tr("URL"));
    if (urlString.isEmpty())
        return;

    QUrl url = QUrl::fromUserInput(urlString);
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid URL."));
        return;
    }

    if (url.port() == -1)
        url.setPort(defaultRtspPort);

    QList<QUrl> urls = m_server->getAdditionalUrls();
    if ((m_server->getNetAddrList().contains(QHostAddress(url.host())) && url.port() == QUrl(m_server->getApiUrl()).port()) || urls.contains(url)) {
        QMessageBox::warning(this, tr("Warning"), tr("This URL is already in the address list."));
        return;
    }

    urls.append(url);

    connection2()->getDiscoveryManager()->addDiscoveryInformation(m_server->getId(), url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    m_server->setAdditionalUrls(urls);
}

void QnRoutingManagementWidget::at_removeButton_clicked() {
    QModelIndex currentIndex = ui->addressesView->currentIndex();

    QModelIndex index = m_sortedServerAddressesModel->mapToSource(currentIndex);
    if (!index.isValid())
        return;

    if (!m_server)
        return;

    QUrl url(index.data(Qt::EditRole).toUrl());
    QList<QUrl> urls = m_server->getAdditionalUrls();
    if (!urls.removeOne(url))
        return;

    connection2()->getDiscoveryManager()->removeDiscoveryInformation(m_server->getId(), url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    m_server->setAdditionalUrls(urls);

    if (m_sortedServerAddressesModel->rowCount() > 0) {
        ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(
                                               qMin(currentIndex.row(), m_sortedServerAddressesModel->rowCount() - 1), currentIndex.column()));
    } else {
        ui->addressesView->setCurrentIndex(QModelIndex());
    }
}

void QnRoutingManagementWidget::at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous);

    if (m_server)
        m_server->disconnect(this);


    QnMediaServerResourcePtr server = current.data(Qn::ResourceRole).value<QnResourcePtr>().dynamicCast<QnMediaServerResource>();
    if (server == m_server)
        return;

    m_server = server;
    updateModel();
    updateUi();

    if (server) {
        connect(server,      &QnMediaServerResource::resourceChanged,    this,   &QnRoutingManagementWidget::updateModel);
        connect(server,      &QnMediaServerResource::auxUrlsChanged,     this,   &QnRoutingManagementWidget::updateModel);
    }
}

void QnRoutingManagementWidget::at_addressesView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid())
        return;

    if (index.column() != QnServerAddressesModel::AddressColumn)
        return;

    if (!m_server)
        return;

    if (!m_serverAddressesModel->isManualAddress(m_sortedServerAddressesModel->mapToSource(index)))
        return;

    QUrl oldUrl = index.data(Qt::EditRole).toUrl();

    QString urlString = QInputDialog::getText(this, tr("Edit URL"), tr("URL"), QLineEdit::Normal, oldUrl.toString());
    if (urlString.isEmpty())
        return;

    QUrl url = QUrl::fromUserInput(urlString);
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid URL."));
        return;
    }

    if (url.port() == -1)
        url.setPort(defaultRtspPort);

    if (oldUrl == url)
        return;

    if (m_server->getNetAddrList().contains(QHostAddress(url.host())) && url.port() == QUrl(m_server->getApiUrl()).port()) {
        QMessageBox::warning(this, tr("Warning"), tr("This URL is already in the address list."));
        return;
    }

    QList<QUrl> urls = m_server->getAdditionalUrls();
    urls.removeOne(oldUrl);
    urls.append(url);
    m_server->setAdditionalUrls(urls);

    connection2()->getDiscoveryManager()->removeDiscoveryInformation(m_server->getId(), oldUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    connection2()->getDiscoveryManager()->addDiscoveryInformation(m_server->getId(), url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
}

void QnRoutingManagementWidget::at_serverAddressesModel_ignoreChangeRequested(const QString &address, bool ignore) {
    if (!m_server)
        return;

    int port = QUrl(m_server->getApiUrl()).port();
    if (port == -1)
        port = defaultRtspPort;

    QUrl url(address);
    if (m_server->getNetAddrList().contains(QHostAddress(url.host())) && url.port() == port)
        url.setPort(-1);

    QList<QUrl> ignoredUrls = m_server->getIgnoredUrls();

    if (ignore) {
        if (ignoredUrls.contains(url))
            return;
        ignoredUrls.append(url);
        connection2()->getDiscoveryManager()->addDiscoveryInformation(m_server->getId(), url, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        if (url.port() == -1)
            ui->warningLabel->show();
    } else {
        if (!ignoredUrls.removeOne(url))
            return;

        if (url.port() == -1)
            connection2()->getDiscoveryManager()->removeDiscoveryInformation(m_server->getId(), url, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        else
            connection2()->getDiscoveryManager()->addDiscoveryInformation(m_server->getId(), url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    m_server->setIgnoredUrls(ignoredUrls);
}

void QnRoutingManagementWidget::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server->getStatus() == Qn::Incompatible)
        return;

    m_serverListModel->addResource(resource);
}

void QnRoutingManagementWidget::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if (resource->hasFlags(Qn::server))
        m_serverListModel->removeResource(resource);

    if (m_server == resource)
        updateModel();
}
