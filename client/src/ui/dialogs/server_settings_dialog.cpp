#include "login_dialog.h"
#include "ui_server_settings_dialog.h"

#include <functional>

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <utils/common/counter.h>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "server_settings_dialog.h"

#include "utils/settings.h"

#define BILLION (1000000000LL)

QnServerSettingsDialog::QnServerSettingsDialog(QnVideoServerResourcePtr server, QWidget *parent):
    QDialog(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server)
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->storagesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    /* Qt designer does not save this flag (probably a bug in Qt designer). */
    ui->storagesTable->horizontalHeader()->setVisible(true);

    connect(ui->addStorageButton,       SIGNAL(clicked()),  this,   SLOT(at_addStorageButton_clicked()));
    connect(ui->removeStorageButton,    SIGNAL(clicked()),  this,   SLOT(at_removeStorageButton_clicked()));

    updateFromResources();
}

QnServerSettingsDialog::~QnServerSettingsDialog() {
    return;
}

void QnServerSettingsDialog::accept() {
    setEnabled(false);
    setCursor(Qt::WaitCursor);

    QString errorString;
    if (!validateStorages(tableStorages(), &errorString)) {
        QMessageBox::warning(this, tr("Warning"), errorString, QMessageBox::Ok);
    } else {
        submitToResources(); 

        QDialog::accept();
    }

    unsetCursor();
    setEnabled(true);
}

void QnServerSettingsDialog::reject() {
    QDialog::reject();
}

int QnServerSettingsDialog::addTableRow(const QString &url, int spaceLimitGb) {
    int row = ui->storagesTable->rowCount();
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *urlItem = new QTableWidgetItem(url);
    QTableWidgetItem *spaceItem = new QTableWidgetItem();
    spaceItem->setData(Qt::DisplayRole, spaceLimitGb);

    ui->storagesTable->setItem(row, 0, urlItem);
    ui->storagesTable->setItem(row, 1, spaceItem);

    ui->storagesTable->openPersistentEditor(spaceItem);

    return row;
}

void QnServerSettingsDialog::setTableStorages(const QnAbstractStorageResourceList &storages) {
    ui->storagesTable->clear();
    ui->storagesTable->setColumnCount(2);

    foreach (const QnAbstractStorageResourcePtr &storage, storages)
        addTableRow(storage->getUrl(), storage->getSpaceLimit() / BILLION);
}

QnAbstractStorageResourceList QnServerSettingsDialog::tableStorages() const {
    QnAbstractStorageResourceList result;

    int rowCount = ui->storagesTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QString path = ui->storagesTable->item(row, 0)->text();
        int spaceLimit = ui->storagesTable->item(row, 1)->data(Qt::DisplayRole).toInt();

        QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
        storage->setName(QUuid::createUuid().toString());
        storage->setParentId(m_server->getId());
        storage->setUrl(path.trimmed());
        storage->setSpaceLimit(spaceLimit * BILLION);

        result.append(storage);
    }

    return result;
}

void QnServerSettingsDialog::updateFromResources() {
    ui->nameEdit->setText(m_server->getName());
    ui->ipAddressEdit->setText(QUrl(m_server->getUrl()).host());
    ui->apiPortEdit->setText(QString::number(QUrl(m_server->getApiUrl()).port()));
    ui->rtspPortEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    setTableStorages(m_server->getStorages());
}

void QnServerSettingsDialog::submitToResources() {
    m_server->setStorages(tableStorages());
    m_server->setName(ui->nameEdit->text());

    QUrl url(m_server->getUrl());
    url.setPort(ui->rtspPortEdit->text().toInt());
    m_server->setUrl(url.toString());

    QUrl apiUrl(m_server->getApiUrl());
    apiUrl.setPort(ui->apiPortEdit->text().toInt());
    m_server->setApiUrl(apiUrl.toString());
}

/*void QnServerSettingsDialog::save()
{
    m_connection->saveAsync(m_server, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}*/

bool QnServerSettingsDialog::validateStorages(const QnAbstractStorageResourceList &storages, QString *errorString) {
    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        if (storage->getUrl().isEmpty()) {
            *errorString = "Storage path must not be empty.";
            return false;
        }

        if (storage->getSpaceLimit() < 0) {
            *errorString = "Space limit must be a non-negative integer.";
            return false;
        }
    }

    QScopedPointer<QnCounter> counter(new QnCounter(storages.size()));
    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());
    connect(counter.data(), SIGNAL(reachedZero()), eventLoop.data(), SLOT(quit()));

    QScopedPointer<detail::CheckPathReplyProcessor> processor(new detail::CheckPathReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived(int, bool, int)), counter.data(), SLOT(decrement()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    QHash<int, QnAbstractStorageResourcePtr> storageByHandle;
    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        int handle = serverConnection->asyncCheckPath(storage->getUrl(), processor.data(), SLOT(processReply(int, bool, int)));
        storageByHandle[handle] = storage;
    }

    eventLoop->exec();

    if(!processor->invalidHandles().empty()) {
        QMessageBox::warning(this, tr("Invalid storage path"), tr("Storage path '%1' is invalid or not accessible for writing.").arg(storageByHandle.value(*processor->invalidHandles().begin())->getUrl()));
        return false;
    }

    return true;
}

#if 0
void ServerSettingsDialog::requestFinished(int status, const QByteArray &/*errorString*/, QnResourceList /*resources*/, int /*handle*/) {
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save server configuration", "Can't save server. Please try again later.");
        ui->buttonBox->setEnabled(true);
    }
}
#endif


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerSettingsDialog::at_addStorageButton_clicked() {
    addTableRow(QString(), 5);
}

void QnServerSettingsDialog::at_removeStorageButton_clicked() {
    QList<int> rows;
    foreach (QModelIndex index, ui->storagesTable->selectionModel()->selectedRows())
        rows.push_back(index.row());

    qSort(rows.begin(), rows.end(), std::greater<int>()); /* Sort in descending order. */
    foreach(int row, rows)
        ui->storagesTable->removeRow(row);
}

