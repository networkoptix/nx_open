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
#include "utils/common/util.h"

namespace {
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

    static void getAddresses(const QnMediaServerResourcePtr &server, QSet<QUrl> &autoUrls, QSet<QUrl> &additionalUrls, QSet<QUrl> &ignoredUrls) {
        for (const QHostAddress &address: server->getNetAddrList()) {
            QUrl url;
            url.setScheme(lit("http"));
            url.setHost(address.toString());
            autoUrls.insert(url);
        }

        for (const QUrl &url: server->getAdditionalUrls())
            additionalUrls.insert(url);

        for (const QUrl &url: server->getIgnoredUrls())
            ignoredUrls.insert(url);
    }
}

class RoutingChange {
public:
    QHash<QUrl, bool> addresses;
    QHash<QUrl, bool> ignoredAddresses;

    void apply(QSet<QUrl> &additionalUrls, QSet<QUrl> &ignoredUrls) const {
        processSet(additionalUrls, addresses);
        processSet(ignoredUrls, ignoredAddresses);
    }

    void simplify(const QSet<QUrl> &autoUrls, const QSet<QUrl> &additionalUrls, const QSet<QUrl> &ignoredUrls, int port) {
        for (auto it = addresses.begin(); it != addresses.end(); /* no inc */) {
            QUrl url = it.key();
            QUrl explicitUrl = url;
            explicitUrl.setPort(port);

            if (autoUrls.contains(url) || it.value() == (additionalUrls.contains(url) || additionalUrls.contains(explicitUrl)))
                it = addresses.erase(it);
            else
                ++it;
        }
        for (auto it = ignoredAddresses.begin(); it != ignoredAddresses.end(); /* no inc */) {
            QUrl url = it.key();
            QUrl explicitUrl = url;
            explicitUrl.setPort(port);

            if (it.value() == (ignoredUrls.contains(url) || ignoredUrls.contains(explicitUrl)))
                it = ignoredAddresses.erase(it);
            else
                ++it;
        }
    }

    static RoutingChange diff(const QSet<QUrl> &additionalUrlsA, const QSet<QUrl> &ignoredUrlsA,
                              const QSet<QUrl> &additionalUrlsB, const QSet<QUrl> &ignoredUrlsB)
    {
        RoutingChange change;

        QSet<QUrl> removed = additionalUrlsA;
        QSet<QUrl> added = additionalUrlsB;
        for (auto it = removed.begin(); it != removed.end(); /* no inc */) {
            QUrl url = *it;
            QUrl implicitUrl = url;
            implicitUrl.setPort(-1);

            if (added.contains(url) || added.contains(implicitUrl)) {
                added.remove(url);
                added.remove(implicitUrl);
                it = removed.erase(it);
            } else {
                ++it;
            }
        }

        for (const QUrl &url: removed)
            change.addresses.insert(url, false);
        for (const QUrl &url: added)
            change.addresses.insert(url, true);

        removed = ignoredUrlsA;
        added = ignoredUrlsB;
        for (auto it = removed.begin(); it != removed.end(); /* no inc */) {
            QUrl url = *it;
            QUrl implicitUrl = url;
            implicitUrl.setPort(-1);

            if (added.contains(url) || added.contains(implicitUrl)) {
                added.remove(url);
                added.remove(implicitUrl);
                it = removed.erase(it);
            } else {
                ++it;
            }
        }

        for (const QUrl &url: removed)
            change.ignoredAddresses.insert(url, false);
        for (const QUrl &url: added)
            change.ignoredAddresses.insert(url, true);

        return change;
    }

private:
    void processSet(QSet<QUrl> &set, const QHash<QUrl, bool> &hash) const {
        for (auto it = hash.begin(); it != hash.end(); ++it) {
            if (it.value())
                set.insert(it.key());
            else
                set.remove(it.key());
        }
    }
};

class RoutingManagementChanges {
public:
    QHash<QnUuid, RoutingChange> changes;
};

QnRoutingManagementWidget::QnRoutingManagementWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnRoutingManagementWidget),
    m_changes(new RoutingManagementChanges)
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
    m_sortedServerAddressesModel->setDynamicSortFilter(true);
    m_sortedServerAddressesModel->setSortRole(Qt::DisplayRole);
    m_sortedServerAddressesModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sortedServerAddressesModel->sort(QnServerAddressesModel::AddressColumn);
    m_sortedServerAddressesModel->setSourceModel(m_serverAddressesModel);
    ui->addressesView->setModel(m_sortedServerAddressesModel);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->addressesView->horizontalHeader()->setSectionResizeMode(QnServerAddressesModel::AddressColumn, QHeaderView::Stretch);
    ui->addressesView->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->serversView->selectionModel(),  &QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::at_serversView_currentIndexChanged);
    connect(ui->addressesView->selectionModel(),&QItemSelectionModel::currentRowChanged,        this,   &QnRoutingManagementWidget::updateUi);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::dataChanged,           this,   &QnRoutingManagementWidget::at_serverAddressesModel_dataChanged);
    connect(m_serverAddressesModel,             &QnServerAddressesModel::urlEditingFailed,      this,   [this](const QModelIndex &, int error) { reportUrlEditingError(error); });
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

void QnRoutingManagementWidget::submitToSettings() {
    ui->warningLabel->hide();

    updateFromModel();

    for (auto it = m_changes->changes.begin(); it != m_changes->changes.end(); ++it) {
        QnUuid serverId = it.key();
        QnMediaServerResourcePtr server = qnResPool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QSet<QUrl> autoUrls;
        QSet<QUrl> additionalUrls;
        QSet<QUrl> ignoredUrls;
        getAddresses(server, autoUrls, additionalUrls, ignoredUrls);

        it->simplify(autoUrls, additionalUrls, ignoredUrls, server->getPort());
        QHash<QUrl, bool> additional = it->addresses;
        QHash<QUrl, bool> ignored = it->ignoredAddresses;

        for (auto it = additional.begin(); it != additional.end(); ++it) {
            QUrl url = it.key();

            if (it.value()) {
                bool ign = false;
                if (ignored.contains(url))
                    ign = ignored.take(url);

                connection2()->getDiscoveryManager()->addDiscoveryInformation(serverId, url, ign, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
            } else {
                ignored.remove(url);
                connection2()->getDiscoveryManager()->removeDiscoveryInformation(serverId, url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
            }
        }
        for (auto it = ignored.begin(); it != ignored.end(); ++it) {
            QUrl url = it.key();

            if (it.value())
                connection2()->getDiscoveryManager()->addDiscoveryInformation(serverId, url, true, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
            else {
                if (autoUrls.contains(url))
                    connection2()->getDiscoveryManager()->removeDiscoveryInformation(serverId, url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
                else
                    connection2()->getDiscoveryManager()->addDiscoveryInformation(serverId, url, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
            }
        }
    }

    m_changes->changes.clear();
}

void QnRoutingManagementWidget::updateModel() {
    if (!m_server) {
        m_serverAddressesModel->clear();
        return;
    }

    QnUuid serverId = m_server->getId();

    QSet<QUrl> autoUrls;
    QSet<QUrl> additionalUrls;
    QSet<QUrl> ignoredUrls;
    getAddresses(m_server, autoUrls, additionalUrls, ignoredUrls);

    m_changes->changes.value(serverId).apply(additionalUrls, ignoredUrls);

    int row = ui->addressesView->currentIndex().row();
    QUrl url = ui->addressesView->currentIndex().data(Qt::EditRole).toUrl();

    m_serverAddressesModel->resetModel(autoUrls.toList(), additionalUrls.toList(), ignoredUrls, m_server->getPort());

    int rowCount = m_serverAddressesModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QModelIndex index = m_serverAddressesModel->index(i, QnServerAddressesModel::AddressColumn);
        if (m_serverAddressesModel->data(index, Qt::EditRole).toUrl() == url) {
            row = m_sortedServerAddressesModel->mapFromSource(index).row();
            break;
        }
    }

    if (row < m_sortedServerAddressesModel->rowCount())
        ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));
}

void QnRoutingManagementWidget::updateFromModel() {
    if (!m_server)
        return;

    QnUuid serverId = m_server->getId();

    QSet<QUrl> autoUrls;
    QSet<QUrl> additionalUrls;
    QSet<QUrl> ignoredUrls;
    getAddresses(m_server, autoUrls, additionalUrls, ignoredUrls);
    m_changes->changes[serverId] = RoutingChange::diff(
                                       additionalUrls, ignoredUrls,
                                       QSet<QUrl>::fromList(m_serverAddressesModel->manualAddressList()),
                                       m_serverAddressesModel->ignoredAddresses());
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
    url.setScheme(lit("http"));

    if (!url.isValid()) {
        reportUrlEditingError(QnServerAddressesModel::InvalidUrl);
        return;
    }

    if (url.port() == m_server->getPort())
        url.setPort(-1);

    QUrl explicitUrl = url;
    explicitUrl.setPort(m_server->getPort());
    if (m_serverAddressesModel->addressList().contains(url) ||
        m_serverAddressesModel->manualAddressList().contains(url) ||
        m_serverAddressesModel->manualAddressList().contains(explicitUrl))
    {
        reportUrlEditingError(QnServerAddressesModel::ExistingUrl);
        return;
    }

    m_serverAddressesModel->addAddress(url);
}

void QnRoutingManagementWidget::at_removeButton_clicked() {
    QModelIndex currentIndex = ui->addressesView->currentIndex();

    QModelIndex index = m_sortedServerAddressesModel->mapToSource(currentIndex);
    if (!index.isValid())
        return;

    if (!m_server)
        return;

    m_serverAddressesModel->removeAddressAtIndex(index);

    int row = qMin(m_sortedServerAddressesModel->rowCount() - 1, currentIndex.row());
    ui->addressesView->setCurrentIndex(m_sortedServerAddressesModel->index(row, 0));
}

void QnRoutingManagementWidget::at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous);

    if (m_server)
        m_server->disconnect(this);

    updateFromModel();

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

void QnRoutingManagementWidget::at_serverAddressesModel_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    if (!m_server)
        return;

    if (topLeft != bottomRight || topLeft.column() != QnServerAddressesModel::InUseColumn)
        return;

    if (topLeft.data(Qt::CheckStateRole).toInt() == Qt::Unchecked) {
        if (!m_serverAddressesModel->isManualAddress(topLeft))
            ui->warningLabel->show();
    }
}

void QnRoutingManagementWidget::reportUrlEditingError(int error) {
    switch (error) {
    case QnServerAddressesModel::InvalidUrl:
        QMessageBox::critical(this, tr("Error"), tr("You have entered an invalid URL."));
        break;
    case QnServerAddressesModel::ExistingUrl:
        QMessageBox::warning(this, tr("Warning"), tr("This URL is already in the address list."));
        break;
    }
}

void QnRoutingManagementWidget::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server->getStatus() == Qn::Incompatible)
        return;

    m_serverListModel->addResource(resource);
}

void QnRoutingManagementWidget::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server || server->getStatus() == Qn::Incompatible)
        return;

    m_serverListModel->removeResource(resource);

    if (m_server == resource)
        updateModel();
}
