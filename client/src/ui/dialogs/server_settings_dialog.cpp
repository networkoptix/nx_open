#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <functional>

#include <QtCore/QUuid>
#include <QtCore/QDir>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QSlider>
#include <QtGui/QLabel>

#include <utils/common/counter.h>
#include <utils/common/string.h>
#include <utils/common/variant.h>
#include <utils/math/interpolator.h>
#include <utils/math/color_transformations.h>

#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/noptix_style.h>

namespace {
    const qint64 defaultReservedSpace = 5ll * 1000ll * 1000ll * 1000ll;

    const qint64 bytesInMib = 1024 * 1024;

    const int SpaceRole = Qt::UserRole;
    const int TotalSpaceRole = Qt::UserRole + 1;
    const int StorageIdRole = Qt::UserRole + 2;

    enum Column {
        CheckBoxColumn,
        PathColumn,
        UsedSpaceColumn,
        ArchiveSpaceColumn
    };

    class SpaceItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        SpaceItemDelegate(QObject *parent = NULL): base_type(parent) {}

        virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            if(!m_gradient.isNull()) {
                qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
                qint64 space = index.data(SpaceRole).toLongLong();
                if(totalSpace > 0) {
                    double fill = qBound(0.0, static_cast<double>(space) / totalSpace, 1.0);
                    QColor color = m_gradient(fill);

                    painter->fillRect(QRect(option.rect.topLeft(), QSize(option.rect.width() * fill, option.rect.height())), color);
                }
            }

            base_type::paint(painter, option, index);
        }

        const QnInterpolator<QColor> &gradient() {
            return m_gradient;
        }

        void setGradient(const QnInterpolator<QColor> &gradient) {
            m_gradient = gradient;
        }

    private:
        QnInterpolator<QColor> m_gradient;
    };

    class UsedSpaceItemDelegate: public SpaceItemDelegate {
        typedef SpaceItemDelegate base_type;
    public:
        UsedSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {
            QVector<QPair<qreal, QColor> > points;
            points 
                << qMakePair( 0.8, QColor(0, 255, 0, 48))
                << qMakePair(0.85, QColor(255, 255, 0, 48))
                << qMakePair( 0.9, QColor(255, 0, 0, 48));
            setGradient(points);
        }
    };

    class ArchiveSpaceSlider: public QSlider {
        typedef QSlider base_type;
    public:
        ArchiveSpaceSlider(QWidget *parent = NULL): 
            base_type(parent),
            m_color(Qt::white)
        {
            setOrientation(Qt::Horizontal);
            setMouseTracking(true);
            setProperty(Qn::SliderLength, 0);
        }

        const QColor &color() const {
            return m_color;
        }

        void setColor(const QColor &color) {
            m_color = color;
        }

    protected:
        virtual void mouseMoveEvent(QMouseEvent *event) override {
            base_type::mouseMoveEvent(event);

            int x = handlePos();
            if(qAbs(x - event->pos().x()) < 6) {
                setCursor(Qt::SplitHCursor);
            } else {
                unsetCursor();
            }
        }

        virtual void leaveEvent(QEvent *event) override {
            unsetCursor();
            
            base_type::leaveEvent(event);
        }

        virtual void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            QRect rect = this->rect();

            painter.fillRect(rect, palette().color(backgroundRole()));

            if(minimum() != maximum()) {
                int x = handlePos();
                painter.fillRect(QRect(QPoint(0, 0), QPoint(x, rect.bottom())), m_color);
                
                painter.setPen(withAlpha(m_color.lighter(), 128));
                painter.drawLine(QPoint(x, 0), QPoint(x, rect.bottom()));
            }

            const int textMargin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, this) + 1;
            QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0); // remove width padding
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, formatFileSize(sliderPosition() * bytesInMib));
        }

    private:
        int handlePos() const {
            if(minimum() == maximum())
                return 0;

            return rect().width() * static_cast<double>(sliderPosition() - minimum()) / (maximum() - minimum());
        }

    private:
        QColor m_color;
    };

    class ArchiveSpaceItemDelegate: public SpaceItemDelegate {
        typedef SpaceItemDelegate base_type;
    public:
        ArchiveSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {
            m_color = toTransparent(withAlpha(qnGlobals->selectionColor(), 255), 0.25);

            QVector<QPair<qreal, QColor> > points;
            points << qMakePair(0.0, m_color);
            setGradient(points);
        }

        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
            ArchiveSpaceSlider *result = new ArchiveSpaceSlider(parent);
            result->setColor(m_color);
            return result;
        }

        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override {
            ArchiveSpaceSlider *slider = dynamic_cast<ArchiveSpaceSlider *>(editor);
            if(!slider) {
                base_type::setEditorData(editor, index);
                return;
            }

            qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
            qint64 space = index.data(SpaceRole).toLongLong();

            slider->setRange(0, totalSpace / bytesInMib);
            slider->setValue(space / bytesInMib);
        }

        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
            ArchiveSpaceSlider *slider = dynamic_cast<ArchiveSpaceSlider *>(editor);
            if(!slider) {
                base_type::setModelData(editor, model, index);
                return;
            }

            qint64 value = slider->value() * bytesInMib;
            model->setData(index, value, SpaceRole);
            model->setData(index, formatFileSize(value), Qt::DisplayRole);
        }

    private:
        QColor m_color;
    };

} // anonymous namespace

struct QnServerSettingsDialog::StorageItem: public QnStorageSpaceData {
    StorageItem() {}
    StorageItem(const QnStorageSpaceData &data): QnStorageSpaceData(data) {}

    qint64 reservedSpace;
    bool inUse;
};


QnServerSettingsDialog::QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_tableBottomLabel(NULL),
    m_hasStorageChanges(false)
{
    ui->setupUi(this);
    
    ui->storagesTable->resizeColumnsToContents();
    ui->storagesTable->horizontalHeader()->setVisible(true); /* Qt designer does not save this flag (probably a bug in Qt designer). */
    ui->storagesTable->horizontalHeader()->setResizeMode(CheckBoxColumn, QHeaderView::Fixed);
    ui->storagesTable->horizontalHeader()->setResizeMode(PathColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setResizeMode(UsedSpaceColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setResizeMode(ArchiveSpaceColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->setItemDelegateForColumn(UsedSpaceColumn, new UsedSpaceItemDelegate(this));
    ui->storagesTable->setItemDelegateForColumn(ArchiveSpaceColumn, new ArchiveSpaceItemDelegate(this));

    setButtonBox(ui->buttonBox);

    /* Set up context help. */
    setHelpTopic(ui->nameLabel,           ui->nameLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->ipAddressLabel,      ui->ipAddressLineEdit,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->portLabel,           ui->portLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->panicModeTextLabel,  ui->panicModeLabel,                 Qn::ServerSettings_Panic_Help);
    setHelpTopic(ui->storagesGroupBox,                                        Qn::ServerSettings_Storages_Help);

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
    int row = ui->storagesTable->rowCount() - 1;
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *checkBoxItem = new QTableWidgetItem();
    checkBoxItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    checkBoxItem->setCheckState(item.inUse ? Qt::Checked : Qt::Unchecked);
    checkBoxItem->setData(StorageIdRole, item.storageId);

    QTableWidgetItem *pathItem = new QTableWidgetItem();
    pathItem->setData(Qt::DisplayRole, item.path);
    pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    QTableWidgetItem *usedSpaceItem = new QTableWidgetItem();
    usedSpaceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if(item.freeSpace == -1 || item.totalSpace == -1) {
        usedSpaceItem->setData(Qt::DisplayRole, tr("Not available"));
        usedSpaceItem->setData(SpaceRole, 0);
        usedSpaceItem->setData(TotalSpaceRole, 0);
    } else {
        /*: This text refers to storage's used space. E.g. "2Gb of 30Gb". */
        usedSpaceItem->setData(Qt::DisplayRole, tr("%1 of %2").arg(formatFileSize(item.totalSpace - item.freeSpace)).arg(formatFileSize(item.totalSpace)));
        usedSpaceItem->setData(SpaceRole, item.totalSpace - item.freeSpace);
        usedSpaceItem->setData(TotalSpaceRole, item.totalSpace);
    }

    QTableWidgetItem *archiveSpaceItem = new QTableWidgetItem();
    archiveSpaceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    if(item.reservedSpace == -1) {
        archiveSpaceItem->setData(Qt::DisplayRole, tr("Not applicable"));
        archiveSpaceItem->setData(SpaceRole, 0);
        archiveSpaceItem->setData(TotalSpaceRole, 0);
    } else {
        archiveSpaceItem->setData(Qt::DisplayRole, formatFileSize(item.totalSpace - item.reservedSpace));
        archiveSpaceItem->setData(SpaceRole, item.totalSpace - item.reservedSpace);
        archiveSpaceItem->setData(TotalSpaceRole, item.totalSpace);
    }

    ui->storagesTable->setItem(row, CheckBoxColumn, checkBoxItem);
    ui->storagesTable->setItem(row, PathColumn, pathItem);
    ui->storagesTable->setItem(row, UsedSpaceColumn, usedSpaceItem);
    ui->storagesTable->setItem(row, ArchiveSpaceColumn, archiveSpaceItem);
    ui->storagesTable->openPersistentEditor(archiveSpaceItem);
}

void QnServerSettingsDialog::setTableItems(const QList<StorageItem> &items) {
    ui->storagesTable->setRowCount(0);
    ui->storagesTable->insertRow(0);
    ui->storagesTable->setSpan(0, 0, 1, 4);
    m_tableBottomLabel = new QLabel();
    m_tableBottomLabel->setAlignment(Qt::AlignCenter);
    ui->storagesTable->setCellWidget(0, 0, m_tableBottomLabel);

    foreach(const StorageItem &item, items)
        addTableItem(item);
}

QList<QnServerSettingsDialog::StorageItem> QnServerSettingsDialog::tableItems() const {
    QList<StorageItem> result;

    for(int row = 0; row < ui->storagesTable->rowCount() - 1; row++) {
        StorageItem item;

        item.inUse = ui->storagesTable->item(row, CheckBoxColumn)->checkState() == Qt::Checked;
        item.storageId = qvariant_cast<int>(ui->storagesTable->item(row, CheckBoxColumn)->data(StorageIdRole), -1);

        item.path = ui->storagesTable->item(row, PathColumn)->text();

        item.totalSpace = ui->storagesTable->item(row, UsedSpaceColumn)->data(TotalSpaceRole).toLongLong();
        item.freeSpace = item.totalSpace - ui->storagesTable->item(row, UsedSpaceColumn)->data(SpaceRole).toLongLong();
        item.reservedSpace = item.totalSpace - ui->storagesTable->item(row, ArchiveSpaceColumn)->data(SpaceRole).toLongLong();

        result.push_back(item);
    }

    return result;
}

void QnServerSettingsDialog::updateFromResources() {
    m_server->apiConnection()->asyncGetStorageSpace(this, SLOT(at_replyReceived(int, const QnStorageSpaceDataList &, int)));
    setTableItems(QList<StorageItem>());
    m_tableBottomLabel->setText(tr("Loading..."));

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
    if(m_hasStorageChanges) {
        QnAbstractStorageResourceList storages;
        foreach(const StorageItem &item, tableItems()) {
            if(!item.inUse)
                continue;

            QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
            if (item.storageId != -1)
                storage->setId(item.storageId);
            storage->setName(QUuid::createUuid().toString());
            storage->setParentId(m_server->getId());
            storage->setUrl(item.path);
            storage->setSpaceLimit(item.reservedSpace);

            storages.push_back(storage);
        }
        m_server->setStorages(storages);
    }

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
    // XXX

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_storagesTable_cellChanged(int row, int column) {
    Q_UNUSED(column)
    Q_UNUSED(row)

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_replyReceived(int status, const QnStorageSpaceDataList &dataList, int handle) {
    if(status != 0) {
        m_tableBottomLabel->setText(tr("Could not load storages from server."));
        return;
    }

    QList<StorageItem> items;

    QnAbstractStorageResourceList storages = m_server->getStorages();
    foreach(const QnStorageSpaceData &data, dataList) {
        StorageItem item = data;

        item.reservedSpace = -1;
        if(item.storageId != -1) {
            foreach(const QnAbstractStorageResourcePtr &storage, storages) {
                if(storage->getId() == item.storageId) {
                    item.reservedSpace = storage->getSpaceLimit();
                    break;
                }
            }
        }
        /* Note that if freeSpace is -1, then we'll also get -1 in reservedSpace, which is the desired behavior. */
        if(item.reservedSpace == -1)
            item.reservedSpace = qMin(defaultReservedSpace, item.freeSpace);

        item.inUse = item.storageId != -1;

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
    m_tableBottomLabel->setText(tr("<a href='1'>Add external storage...</a>"));
}
