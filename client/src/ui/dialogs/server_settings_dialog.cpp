#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <functional>

#include <QtCore/QUuid>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QSpinBox>

#include <utils/common/counter.h>

#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>

static const qint64 MIN_RECORD_FREE_SPACE = 1000ll * 1000ll * 1000ll * 5;

namespace {
    const int defaultSpaceLimitGb = 5;
    const qint64 BILLION = 1000000000LL;

    class StorageSettingsItemEditorFactory: public QItemEditorFactory {
    public:
        virtual QWidget *createEditor(QVariant::Type type, QWidget *parent) const override {
            QWidget *result = QItemEditorFactory::createEditor(type, parent);

            if(QSpinBox *spinBox = dynamic_cast<QSpinBox *>(result))
                spinBox->setRange(0, 10000); /* That's for space limit. */

            return result;
        }
    };

} // anonymous namespace

QnServerSettingsDialog::QnServerSettingsDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_hasStorageChanges(false)
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->storagesTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setVisible(true); /* Qt designer does not save this flag (probably a bug in Qt designer). */

    QStyledItemDelegate *itemDelegate = new QStyledItemDelegate(this);
    itemDelegate->setItemEditorFactory(new StorageSettingsItemEditorFactory());
    ui->storagesTable->setItemDelegate(itemDelegate);

    setButtonBox(ui->buttonBox);

    connect(ui->storageAddButton,       SIGNAL(clicked()),              this,   SLOT(at_storageAddButton_clicked()));
    connect(ui->storageRemoveButton,    SIGNAL(clicked()),              this,   SLOT(at_storageRemoveButton_clicked()));
    connect(ui->storagesTable,          SIGNAL(cellChanged(int, int)),  this,   SLOT(at_storagesTable_cellChanged(int, int)));

    updateFromResources();
}

QnServerSettingsDialog::~QnServerSettingsDialog() {
    return;
}

void QnServerSettingsDialog::accept() {
    setEnabled(false);
    setCursor(Qt::WaitCursor);

    bool valid = m_hasStorageChanges ? validateStorages(tableStorages()) : true;
    if (valid) {
        submitToResources(); 

        base_type::accept();
    }

    unsetCursor();
    setEnabled(true);
}

void QnServerSettingsDialog::reject() {
    base_type::reject();
}

int QnServerSettingsDialog::addTableRow(int id, const QString &url, int spaceLimitGb) {
    int row = ui->storagesTable->rowCount();
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *urlItem = new QTableWidgetItem(url);
    urlItem->setData(Qt::UserRole, id);
    QTableWidgetItem *spaceItem = new QTableWidgetItem();
    spaceItem->setData(Qt::DisplayRole, spaceLimitGb);

    ui->storagesTable->setItem(row, 0, urlItem);
    ui->storagesTable->setItem(row, 1, spaceItem);

    updateSpaceLimitCell(row, true);

    return row;
}

void QnServerSettingsDialog::setTableStorages(const QnAbstractStorageResourceList &storages) {
    ui->storagesTable->setRowCount(0);
    ui->storagesTable->setColumnCount(2);

    foreach (const QnAbstractStorageResourcePtr &storage, storages)
        addTableRow(storage->getId().toInt(), storage->getUrl(), storage->getSpaceLimit() / BILLION);
}

QnAbstractStorageResourceList QnServerSettingsDialog::tableStorages() const {
    QnAbstractStorageResourceList result;

    int rowCount = ui->storagesTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        int id = ui->storagesTable->item(row, 0)->data(Qt::UserRole).toInt();
        QString path = ui->storagesTable->item(row, 0)->text();
        int spaceLimit = ui->storagesTable->item(row, 1)->data(Qt::DisplayRole).toInt();

        QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
        if (id)
            storage->setId(id);
        storage->setName(QUuid::createUuid().toString());
        storage->setParentId(m_server->getId());
        storage->setUrl(path.trimmed());
        storage->setSpaceLimit(spaceLimit * BILLION);

        result.append(storage);
    }

    return result;
}

void QnServerSettingsDialog::updateFromResources() {
    ui->nameLineEdit->setText(m_server->getName());
    ui->ipAddressLineEdit->setText(QUrl(m_server->getUrl()).host());
    ui->portLineEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    bool panicMode = m_server->isPanicMode();
    ui->panicModeLabel->setText(panicMode ? tr("On") : tr("Off"));
    {
        QPalette palette = this->palette();
        if(panicMode)
            palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
        ui->panicModeLabel->setPalette(palette);
    }

    setTableStorages(m_server->getStorages());

    m_hasStorageChanges = false;
}

void QnServerSettingsDialog::submitToResources() {
    m_server->setStorages(tableStorages());
    m_server->setName(ui->nameLineEdit->text());
}

QString formatGbStr(qint64 value)
{
    return QString::number(value/1000000000.0, 'f', 2);
}

bool QnServerSettingsDialog::validateStorages(const QnAbstractStorageResourceList &storages) {
    if(storages.isEmpty()) {
        QMessageBox::critical(this, tr("No storages specified"), tr("At least one storage must be specified."));
        return false;
    }

    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        if (storage->getUrl().isEmpty()) {
            QMessageBox::critical(this, tr("Invalid storage path"), tr("Storage path must not be empty."));
            return false;
        }

        if (storage->getSpaceLimit() < 0) {
            QMessageBox::critical(this, tr("Invalid space limit"), tr("Space limit must be a non-negative integer."));
            return false;
        }
    }

    QScopedPointer<QnCounter> counter(new QnCounter(storages.size()));
    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());
    connect(counter.data(), SIGNAL(reachedZero()), eventLoop.data(), SLOT(quit()));

    QScopedPointer<detail::CheckFreeSpaceReplyProcessor> processor(new detail::CheckFreeSpaceReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived(int, qint64, qint64, int)), counter.data(), SLOT(decrement()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    QHash<int, QnAbstractStorageResourcePtr> storageByHandle;
    foreach (const QnAbstractStorageResourcePtr &storage, storages) {
        int handle = serverConnection->asyncGetFreeSpace(storage->getUrl(), processor.data(), SLOT(processReply(int, qint64, qint64, int)));
        storageByHandle[handle] = storage;
    }

    eventLoop->exec();

    detail::FreeSpaceMap freeSpaceMap = processor->freeSpaceInfo();
    for (detail::FreeSpaceMap::const_iterator itr = freeSpaceMap.constBegin(); itr != freeSpaceMap.constEnd(); ++itr)
    {
        QnAbstractStorageResourcePtr storage = storageByHandle.value(itr.key());
        if (!storage)
            continue;

        qint64 availableSpace = itr.value().freeSpace + itr.value().usedSpace - storage->getSpaceLimit();
        if (itr.value().errorCode == detail::INVALID_PATH)
        {
            QMessageBox::critical(this, tr("Invalid storage path"), tr("Storage path '%1' is invalid or is not accessible for writing.").arg(storage->getUrl()));
            return false;
        }
        if (itr.value().errorCode == detail::SERVER_ERROR)
        {
            QMessageBox::critical(this, tr("Can't verify storage path"), tr("Cannot verify storage path '%1'. Cannot establish connection to the media server.").arg(storage->getUrl()));
            return false;
        }
        else if (availableSpace < 0)
        {
            QMessageBox::critical(this, tr("Not enough disk space"),
                tr("Storage '%1'\nYou have less storage space available than reserved free space value. Additional %2Gb are required.")
                .arg(storage->getUrl())
                .arg(formatGbStr(MIN_RECORD_FREE_SPACE - availableSpace)));
            return false;
        }
        else if (availableSpace < MIN_RECORD_FREE_SPACE)
        {
            QMessageBox::warning(this, tr("Low space for archive"),
                tr("Storage '%1'\nYou have only %2Gb left for video archive.")
                .arg(storage->getUrl())
                .arg(formatGbStr(availableSpace)));
        }
    }

    return true;
}

void QnServerSettingsDialog::updateSpaceLimitCell(int row, bool force) {
    QTableWidgetItem *urlItem = ui->storagesTable->item(row, 0);
    QTableWidgetItem *spaceItem = ui->storagesTable->item(row, 1);
    if(!urlItem || !spaceItem)
        return;

    QString url = urlItem->data(Qt::DisplayRole).toString();

    bool newSupportsSpaceLimit = !url.trimmed().startsWith(QLatin1String("coldstore://")); // TODO: evil hack, move out the check somewhere into storage factory.
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
void QnServerSettingsDialog::at_storageAddButton_clicked() {
    addTableRow(0, QString(), defaultSpaceLimitGb);

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_storageRemoveButton_clicked() {
    QList<int> rows;
    foreach (QModelIndex index, ui->storagesTable->selectionModel()->selectedRows())
        rows.push_back(index.row());

    qSort(rows.begin(), rows.end(), std::greater<int>()); /* Sort in descending order. */
    foreach(int row, rows)
        ui->storagesTable->removeRow(row);

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_storagesTable_cellChanged(int row, int column) {
    Q_UNUSED(column)
    updateSpaceLimitCell(row);

    m_hasStorageChanges = true;
}
