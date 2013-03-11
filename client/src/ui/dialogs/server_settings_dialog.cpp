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

#include <client/client_model_types.h>
#include <utils/settings.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/noptix_style.h>

#include "storage_url_dialog.h"

//#define QN_SHOW_ARCHIVE_SPACE_COLUMN

namespace {
    const qint64 defaultReservedSpace = 5ll * 1000ll * 1000ll * 1000ll;
    const qint64 minimalReservedSpace = 5ll * 1000ll * 1000ll * 1000ll;

    const qint64 bytesInMb = 1000 * 1000;

    const int ReservedSpaceRole = Qt::UserRole;
    const int FreeSpaceRole = Qt::UserRole + 1;
    const int TotalSpaceRole = Qt::UserRole + 2;
    const int StorageIdRole = Qt::UserRole + 3;
    const int ExternalRole = Qt::UserRole + 4;

    enum Column {
        CheckBoxColumn,
        PathColumn,
        CapacityColumn,
        ArchiveSpaceColumn,
        ColumnCount
    };

    QString formatStorageSize(qint64 size) {
        return formatFileSize(size, 1, 10);
    }

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

            setTextFormat(QLatin1String("%1"));

            connect(this, SIGNAL(sliderPressed()), this, SLOT(update()));
            connect(this, SIGNAL(sliderReleased()), this, SLOT(update()));
        }

        const QColor &color() const {
            return m_color;
        }

        void setColor(const QColor &color) {
            m_color = color;
        }

        QString text() const {
            if(!m_textFormatHasPlaceholder) {
                return m_textFormat;
            } else {
                if(isSliderDown()) {
                    return formatStorageSize(sliderPosition() * bytesInMb);
                } else {
                    return tr("%1%").arg(static_cast<int>(relativePosition() * 100));
                }
            }
        }

        QString textFormat() const {
            return m_textFormat;
        }

        void setTextFormat(const QString &textFormat) {
            if(m_textFormat == textFormat)
                return;

            m_textFormat = textFormat;
            m_textFormatHasPlaceholder = textFormat.contains(QLatin1String("%1"));
            update();
        }

    protected:
        virtual void mouseMoveEvent(QMouseEvent *event) override {
            base_type::mouseMoveEvent(event);

            if(!isEmpty()) {
                int x = handlePos();
                if(qAbs(x - event->pos().x()) < 6) {
                    setCursor(Qt::SplitHCursor);
                } else {
                    unsetCursor();
                }
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

            if(!isEmpty()) {
                int x = handlePos();
                painter.fillRect(QRect(QPoint(0, 0), QPoint(x, rect.bottom())), m_color);
                
                painter.setPen(withAlpha(m_color.lighter(), 128));
                painter.drawLine(QPoint(x, 0), QPoint(x, rect.bottom()));
            }

            const int textMargin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, this) + 1;
            QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0);
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
        }

    private:
        qreal relativePosition() const {
            return isEmpty() ? 0.0 : static_cast<double>(sliderPosition() - minimum()) / (maximum() - minimum());
        }

        int handlePos() const {
            return rect().width() * relativePosition();
        }

        bool isEmpty() const {
            return maximum() == minimum();
        }

    private:
        QColor m_color;
        QString m_textFormat;
        bool m_textFormatHasPlaceholder;
    };

    class ArchiveSpaceItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        ArchiveSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {
            m_color = toTransparent(withAlpha(qnGlobals->selectionColor(), 255), 0.25);
        }

        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
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
            qint64 videoSpace = totalSpace - index.data(ReservedSpaceRole).toLongLong();

            slider->setRange(0, totalSpace / bytesInMb);
            slider->setValue(videoSpace / bytesInMb);
        }

        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
            ArchiveSpaceSlider *slider = dynamic_cast<ArchiveSpaceSlider *>(editor);
            if(!slider) {
                base_type::setModelData(editor, model, index);
                return;
            }

            qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
            qint64 videoSpace = slider->value() * bytesInMb;
            model->setData(index, totalSpace - videoSpace, ReservedSpaceRole);
        }

    private:
        QColor m_color;
    };

} // anonymous namespace


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
    ui->storagesTable->horizontalHeader()->setResizeMode(CapacityColumn, QHeaderView::ResizeToContents);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    ui->storagesTable->horizontalHeader()->setResizeMode(ArchiveSpaceColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->setItemDelegateForColumn(ArchiveSpaceColumn, new ArchiveSpaceItemDelegate(this));
#else
    ui->storagesTable->setColumnCount(ColumnCount - 1);
#endif

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

void QnServerSettingsDialog::addTableItem(const QnStorageSpaceData &item) {
    int row = ui->storagesTable->rowCount() - 1;
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *checkBoxItem = new QTableWidgetItem();
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    if (item.isWritable)
        flags |= Qt::ItemIsEnabled;
    checkBoxItem->setFlags(flags);
    checkBoxItem->setCheckState(item.isUsedForWriting ? Qt::Checked : Qt::Unchecked);
    checkBoxItem->setData(StorageIdRole, item.storageId);
    checkBoxItem->setData(ExternalRole, item.isExternal);

    QTableWidgetItem *pathItem = new QTableWidgetItem();
    pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    pathItem->setData(Qt::DisplayRole, item.path);

    QTableWidgetItem *capacityItem = new QTableWidgetItem();
    capacityItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    capacityItem->setData(Qt::DisplayRole, item.totalSpace == -1 ? tr("Not available") : formatStorageSize(item.totalSpace));

#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    QTableWidgetItem *archiveSpaceItem = new QTableWidgetItem();
    archiveSpaceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
#else
    QTableWidgetItem *archiveSpaceItem = capacityItem;
#endif
    archiveSpaceItem->setData(ReservedSpaceRole, item.reservedSpace);
    archiveSpaceItem->setData(TotalSpaceRole, item.totalSpace);
    archiveSpaceItem->setData(FreeSpaceRole, item.freeSpace);

    ui->storagesTable->setItem(row, CheckBoxColumn, checkBoxItem);
    ui->storagesTable->setItem(row, PathColumn, pathItem);
    ui->storagesTable->setItem(row, CapacityColumn, capacityItem);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    ui->storagesTable->setItem(row, ArchiveSpaceColumn, archiveSpaceItem);
    ui->storagesTable->openPersistentEditor(archiveSpaceItem);
#endif
}

void QnServerSettingsDialog::setTableItems(const QList<QnStorageSpaceData> &items) {
    ui->storagesTable->setRowCount(0);
    ui->storagesTable->insertRow(0);
    ui->storagesTable->setSpan(0, 0, 1, 4);
    m_tableBottomLabel = new QLabel();
    m_tableBottomLabel->setAlignment(Qt::AlignCenter);
    connect(m_tableBottomLabel, SIGNAL(linkActivated(const QString &)), this, SLOT(at_tableBottomLabel_linkActivated()));
    ui->storagesTable->setCellWidget(0, 0, m_tableBottomLabel);

    foreach(const QnStorageSpaceData &item, items)
        addTableItem(item);
}

QList<QnStorageSpaceData> QnServerSettingsDialog::tableItems() const {
    QList<QnStorageSpaceData> result;

    for(int row = 0; row < ui->storagesTable->rowCount() - 1; row++) {
        QnStorageSpaceData item;

        QTableWidgetItem *checkBoxItem = ui->storagesTable->item(row, CheckBoxColumn);
        QTableWidgetItem *pathItem = ui->storagesTable->item(row, PathColumn);
        QTableWidgetItem *capacityItem = ui->storagesTable->item(row, CapacityColumn);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
        QTableWidgetItem *archiveSpaceItem = ui->storagesTable->item(row, ArchiveSpaceColumn);
#else
        QTableWidgetItem *archiveSpaceItem = capacityItem;
#endif

        item.isWritable = checkBoxItem->flags() & Qt::ItemIsEnabled;
        item.isUsedForWriting = checkBoxItem->checkState() == Qt::Checked;
        item.storageId = qvariant_cast<int>(checkBoxItem->data(StorageIdRole), -1);
        item.isExternal = qvariant_cast<bool>(checkBoxItem->data(ExternalRole), true);

        item.path = pathItem->text();

        item.totalSpace = archiveSpaceItem->data(TotalSpaceRole).toLongLong();
        item.freeSpace = archiveSpaceItem->data(FreeSpaceRole).toLongLong();
        item.reservedSpace = archiveSpaceItem->data(ReservedSpaceRole).toLongLong();

        result.push_back(item);
    }

    return result;
}

void QnServerSettingsDialog::updateFromResources() {
    m_server->apiConnection()->asyncGetStorageSpace(this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
    setTableItems(QList<QnStorageSpaceData>());
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
        QnServerStorageStateHash serverStorageStates = qnSettings->serverStorageStates();

        QnAbstractStorageResourceList storages;
        foreach(const QnStorageSpaceData &item, tableItems()) {
            if(!item.isUsedForWriting && item.storageId == -1) {
                serverStorageStates.insert(QnServerStorageKey(m_server->getGuid(), item.path), item.reservedSpace);
                continue;
            }

            QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
            if (item.storageId != -1)
                storage->setId(item.storageId);
            storage->setName(QUuid::createUuid().toString());
            storage->setParentId(m_server->getId());
            storage->setUrl(item.path);
            storage->setSpaceLimit(qMax(minimalReservedSpace, item.reservedSpace)); // TODO: #Elric is this a proper place for this qMax?
            storage->setUsedForWriting(item.isUsedForWriting);

            storages.push_back(storage);
        }
        m_server->setStorages(storages);

        qnSettings->setServerStorageStates(serverStorageStates);
    }

    m_server->setName(ui->nameLineEdit->text());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerSettingsDialog::at_tableBottomLabel_linkActivated() {
    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(m_storageProtocols);
    if(dialog->exec() != QDialog::Accepted)
        return;

    QnStorageSpaceData item = dialog->storage();
    if(item.storageId != -1)
        return;
    item.isUsedForWriting = true;
    item.isExternal = true;
    
    addTableItem(item);
    
    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_storagesTable_cellChanged(int row, int column) {
    Q_UNUSED(column)
    Q_UNUSED(row)

    m_hasStorageChanges = true;
}

void QnServerSettingsDialog::at_replyReceived(int status, const QnStorageSpaceReply &reply, int handle) {
    if(status != 0) {
        m_tableBottomLabel->setText(tr("Could not load storages from server."));
        return;
    }

    QnServerStorageStateHash serverStorageStates = qnSettings->serverStorageStates();

    QList<QnStorageSpaceData> items = reply.storages;
    for(int i = 0; i < items.size(); i++) {
        QnStorageSpaceData &item = items[i];

        if(item.reservedSpace == -1)
            item.reservedSpace = serverStorageStates.value(QnServerStorageKey(m_server->getGuid(), item.path) , -1);

        /* Note that if freeSpace is -1, then we'll also get -1 in reservedSpace, which is the desired behavior. */
        if(item.reservedSpace == -1)
            item.reservedSpace = qMin(defaultReservedSpace, item.freeSpace);
    }

    struct StorageSpaceDataLess {
        bool operator()(const QnStorageSpaceData &l, const QnStorageSpaceData &r) {
            bool lLocal = l.path.contains(lit("://"));
            bool rLocal = r.path.contains(lit("://"));

            if(lLocal != rLocal)
                return lLocal;

            return l.path < r.path;
        }
    };
    qSort(items.begin(), items.end(), StorageSpaceDataLess());

    setTableItems(items);

    m_storageProtocols = reply.storageProtocols;
    m_tableBottomLabel->setText(tr("<a href='1'>Add external Storage...</a>"));
}

