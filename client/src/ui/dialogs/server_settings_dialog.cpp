#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <functional>

#include <QtCore/QUuid>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <utils/common/counter.h>

#include <core/resource/storage_resource.h>
#include <core/resource/video_server.h>

namespace {
    const int defaultSpaceLimitGb = 5;
    const qint64 BILLION = 1000000000LL;
}

QnServerSettingsDialog::QnServerSettingsDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server)
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->storagesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    /* Qt designer does not save this flag (probably a bug in Qt designer). */
    ui->storagesTable->horizontalHeader()->setVisible(true);

    setButtonBox(ui->buttonBox);

    connect(ui->addStorageButton,       SIGNAL(clicked()),              this,   SLOT(at_addStorageButton_clicked()));
    connect(ui->removeStorageButton,    SIGNAL(clicked()),              this,   SLOT(at_removeStorageButton_clicked()));
    connect(ui->storagesTable,          SIGNAL(cellChanged(int, int)),  this,   SLOT(at_storagesTable_cellChanged(int, int)));

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

        base_type::accept();
    }

    unsetCursor();
    setEnabled(true);
}

void QnServerSettingsDialog::reject() {
    base_type::reject();
}

int QnServerSettingsDialog::addTableRow(const QString &url, int spaceLimitGb) {
    int row = ui->storagesTable->rowCount();
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *urlItem = new QTableWidgetItem(url);
    QTableWidgetItem *spaceItem = new QTableWidgetItem();
    spaceItem->setData(Qt::DisplayRole, spaceLimitGb);

    ui->storagesTable->setItem(row, 0, urlItem);
    ui->storagesTable->setItem(row, 1, spaceItem);

    updateSpaceLimitCell(row, true);

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

void QnServerSettingsDialog::updateSpaceLimitCell(int row, bool force) {
    QTableWidgetItem *urlItem = ui->storagesTable->item(row, 0);
    QTableWidgetItem *spaceItem = ui->storagesTable->item(row, 1);
    if(!urlItem || !spaceItem)
        return;

    QString url = urlItem->data(Qt::DisplayRole).toString();

    bool newSupportsSpaceLimit = !url.trimmed().startsWith("coldstore://"); // TODO: evil hack, move out the check somewhere into storage factory.
    bool oldSupportsSpaceLimit = spaceItem->flags() & Qt::ItemIsEditable;
    if(newSupportsSpaceLimit != oldSupportsSpaceLimit || force) {
        spaceItem->setFlags(newSupportsSpaceLimit ? (spaceItem->flags() | Qt::ItemIsEditable) : (spaceItem->flags() & ~Qt::ItemIsEditable));
        
        /* Adjust value. */
        if(newSupportsSpaceLimit) {
            bool ok;
            int spaceLimitGb = spaceItem->data(Qt::DisplayRole).toInt(&ok);
            if(!ok)
                spaceLimitGb = defaultSpaceLimitGb;
            spaceItem->setData(Qt::DisplayRole, spaceLimitGb);
        } else {
            spaceItem->setData(Qt::DisplayRole, QVariant());
        }
        
        /* Open/close editor. */
        if(newSupportsSpaceLimit) {
            ui->storagesTable->openPersistentEditor(spaceItem);
        } else {
            ui->storagesTable->closePersistentEditor(spaceItem);
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerSettingsDialog::at_addStorageButton_clicked() {
    addTableRow(QString(), defaultSpaceLimitGb);
}

void QnServerSettingsDialog::at_removeStorageButton_clicked() {
    QList<int> rows;
    foreach (QModelIndex index, ui->storagesTable->selectionModel()->selectedRows())
        rows.push_back(index.row());

    qSort(rows.begin(), rows.end(), std::greater<int>()); /* Sort in descending order. */
    foreach(int row, rows)
        ui->storagesTable->removeRow(row);
}

void QnServerSettingsDialog::at_storagesTable_cellChanged(int row, int column) {
    if(column != 0)
        return;
    
    updateSpaceLimitCell(row);
}
