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
#include <utils/common/string.h>
#include <utils/math/interpolator.h>

#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

namespace {
    QString formatGbStr(qint64 value) {
        return QString::number(value/1000000000.0, 'f', 2);
    }

    const qint64 defaultReservedSpace = 5ll * 1000ll * 1000ll * 1000ll;

    class StorageSettingsItemEditorFactory: public QItemEditorFactory {
    public:
        virtual QWidget *createEditor(QVariant::Type type, QWidget *parent) const override {
            QWidget *result = QItemEditorFactory::createEditor(type, parent);

            if(QSpinBox *spinBox = dynamic_cast<QSpinBox *>(result))
                spinBox->setRange(0, 10000); /* That's for space limit. */

            return result;
        }
    };

    const int SpaceRole = Qt::UserRole;
    const int TotalSpaceRole = Qt::UserRole;

    class FreeSpaceItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        FreeSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {
            QVector<QPair<qreal, QColor> > points;
            points 
                << qMakePair(0.0, QColor(0, 255, 0, 128))
                << qMakePair(0.5, QColor(255, 255, 0, 128))
                << qMakePair(1.0, QColor(255, 0, 0, 128));
            m_gradient.setPoints()
        }

        virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
            qint64 freeSpace = index.data(SpaceRole).toLongLong();
            if(totalSpace > 0) {
                double fill = qBound(0.0, static_cast<double>(totalSpace - freeSpace) / totalSpace, 1.0);
                QColor color = m_gradient(fill);
                
                painter->fillRect(QRect(option.rect.topLeft(), QSize(option.rect.width() * fill, option.rect.height())), color);
            }

            base_type::paint(painter, option, index);
        }

    private:
        QnInterpolator<QColor> m_gradient;
    };

} // anonymous namespace

struct QnServerSettingsDialog::StorageItem: public QnStorageSpaceData {
    StorageItem() {}
    StorageItem(const QnStorageSpaceData &data): QnStorageSpaceData(data) {}

    qint64 reservedSpace;
};


QnServerSettingsDialog::QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_hasStorageChanges(false)
{
    ui->setupUi(this);
    ui->storagesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    for(int col = 0; col < ui->storagesTable->columnCount(); col++)
        ui->storagesTable->horizontalHeader()->setResizeMode(col, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setVisible(true); /* Qt designer does not save this flag (probably a bug in Qt designer). */

    /*QStyledItemDelegate *itemDelegate = new QStyledItemDelegate(this);
    itemDelegate->setItemEditorFactory(new StorageSettingsItemEditorFactory());
    ui->storagesTable->setItemDelegate(itemDelegate);*/
    //ui->storagesTable->setItemDelegateForRow(2, new FreeSpaceItemDelegate(this));

    setButtonBox(ui->buttonBox);

    /* Set up context help. */
    setHelpTopic(ui->nameLabel,           ui->nameLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->ipAddressLabel,      ui->ipAddressLineEdit,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->portLabel,           ui->portLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->panicModeTextLabel,  ui->panicModeLabel,                 Qn::ServerSettings_Panic_Help);
    setHelpTopic(ui->storagesGroupBox,                                        Qn::ServerSettings_Storages_Help);

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

    bool valid = true;//m_hasStorageChanges ? validateStorages(tableStorages()) : true;
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

void QnServerSettingsDialog::addTableItem(const StorageItem &item) {
    int row = ui->storagesTable->rowCount();
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *checkBoxItem = new QTableWidgetItem();
    checkBoxItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    checkBoxItem->setCheckState(item.storageId == -1 ? Qt::Unchecked : Qt::Checked);

    QTableWidgetItem *pathItem = new QTableWidgetItem();
    pathItem->setData(Qt::DisplayRole, item.path);
    pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    QTableWidgetItem *spaceItem = new QTableWidgetItem();
    spaceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if(item.freeSpace == -1 || item.totalSpace == -1) {
        spaceItem->setData(Qt::DisplayRole, tr("Not available"));
        spaceItem->setData(SpaceRole, 0);
        spaceItem->setData(TotalSpaceRole, 0);
    } else {
        /*: This text refers to storage's free space. E.g. "2Gb of 30Gb". */
        spaceItem->setData(Qt::DisplayRole, tr("%1 of %2").arg(formatFileSize(item.freeSpace)).arg(formatFileSize(item.totalSpace)));
        spaceItem->setData(SpaceRole, item.freeSpace);
        spaceItem->setData(TotalSpaceRole, item.totalSpace);
    }

    QTableWidgetItem *reservedItem = new QTableWidgetItem();
    reservedItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    if(item.reservedSpace == -1) {
        reservedItem->setData(Qt::DisplayRole, tr("Not applicable"));
        reservedItem->setData(SpaceRole, 0);
        reservedItem->setData(TotalSpaceRole, 0);
    } else {
        reservedItem->setData(Qt::DisplayRole, formatFileSize(item.reservedSpace));
        reservedItem->setData(SpaceRole, item.reservedSpace);
        reservedItem->setData(TotalSpaceRole, item.totalSpace);
    }

    ui->storagesTable->setItem(row, 0, checkBoxItem);
    ui->storagesTable->setItem(row, 1, pathItem);
    ui->storagesTable->setItem(row, 2, spaceItem);
    ui->storagesTable->setItem(row, 3, reservedItem);
}

void QnServerSettingsDialog::setTableItems(const QList<StorageItem> &items) {
    ui->storagesTable->clearContents();
    foreach(const StorageItem &item, items)
        addTableItem(item);
}

QList<QnServerSettingsDialog::StorageItem> &QnServerSettingsDialog::items() const {
    return QList<QnServerSettingsDialog::StorageItem>();
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

    //updateSpaceLimitCell(row, true);

    return row;
}


#if 0
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
#endif

void QnServerSettingsDialog::updateFromResources() {
    m_server->apiConnection()->asyncGetStorageSpace(this, SLOT(at_replyReceived(int, const QnStorageSpaceDataList &, int)));
    ui->storagesTable->clearContents();
    // TODO: Loading...

    ui->nameLineEdit->setText(m_server->getName());
    ui->ipAddressLineEdit->setText(QUrl(m_server->getUrl()).host());
    ui->portLineEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    bool panicMode = m_server->getPanicMode() != QnMediaServerResource::PM_None;
    ui->panicModeLabel->setText(panicMode ? tr("On") : tr("Off"));
    {
        QPalette palette = this->palette();
        if(panicMode)
            palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
        ui->panicModeLabel->setPalette(palette);
    }

    m_hasStorageChanges = false;
}

void QnServerSettingsDialog::submitToResources() {
    //m_server->setStorages(tableStorages());
    m_server->setName(ui->nameLineEdit->text());
}

#if 0
bool QnServerSettingsDialog::validateStorages(const QnAbstractStorageResourceList &storages) {
    return true;

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

    QnMediaServerConnectionPtr serverConnection = m_server->apiConnection();
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

        //qint64 availableSpace = itr.value().freeSpace + itr.value().usedSpace - storage->getSpaceLimit();
        qint64 availableSpace = itr.value().totalSpace;
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
        else if (availableSpace < storage->getSpaceLimit())
        {
            QMessageBox::critical(this, tr("Not enough disk space"),
                tr("Storage '%1'\nYou have less storage space available than reserved free space value. Additional %2Gb are required.")
                .arg(storage->getUrl())
                .arg(formatGbStr(storage->getSpaceLimit() - availableSpace)));
            return false;
        }
        /*
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
        */
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
#endif


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerSettingsDialog::at_storageAddButton_clicked() {
    //addTableRow(0, QString(), defaultSpaceLimitGb);

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
    //updateSpaceLimitCell(row);

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_replyReceived(int status, const QnStorageSpaceDataList &dataList, int handle) {
    QList<StorageItem> items;

    QnAbstractStorageResourceList storages = m_server->getStorages();
    foreach(const QnStorageSpaceData &data, dataList) {
        StorageItem item = data;

        item.reservedSpace = -1;
        foreach(const QnAbstractStorageResourcePtr &storage, storages) {
            if(storage->getId() == item.storageId) {
                item.reservedSpace = storage->getSpaceLimit();
                break;
            }
        }

        /* Note that if freeSpace is -1, then we'll also get -1 in reservedSpace, which is the desired behavior. */
        if(item.reservedSpace == -1)
            item.reservedSpace = qMin(defaultReservedSpace, item.freeSpace);

        items.push_back(item);
    }

    struct StorageItemLess {
        bool operator()(const StorageItem &l, const StorageItem &r) {
            bool lLocal = l.path.contains(lit("://"));
            bool rLocal = r.path.contains(lit("://"));

            if(lLocal != rLocal)
                return lLocal;

            return l.path < r.path;
        }
    };
    qSort(items.begin(), items.end(), StorageItemLess());

    setTableItems(items);
}
