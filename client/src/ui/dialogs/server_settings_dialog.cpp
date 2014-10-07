#include "server_settings_dialog.h"
#include "ui_server_settings_dialog.h"

#include <functional>

#include <utils/common/uuid.h>
#include <QtCore/QDir>
#include <QtWidgets/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLabel>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <api/model/storage_space_reply.h>

#include <utils/common/counter.h>
#include <utils/common/string.h>
#include <utils/common/variant.h>
#include <utils/common/event_processors.h>
#include <utils/math/interpolator.h>
#include <utils/math/color_transformations.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <client/client_model_types.h>
#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/noptix_style.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>

//#define QN_SHOW_ARCHIVE_SPACE_COLUMN

namespace {
    //setting free space to zero, since now client does not change this value, so it must keep current value
    const qint64 defaultReservedSpace = 0;  //5ll * 1024ll * 1024ll * 1024ll;

    const qint64 bytesInMiB = 1024 * 1024;

    const int ReservedSpaceRole = Qt::UserRole;
    const int FreeSpaceRole = Qt::UserRole + 1;
    const int TotalSpaceRole = Qt::UserRole + 2;
    const int StorageIdRole = Qt::UserRole + 3;
    const int ExternalRole = Qt::UserRole + 4;

    enum Column {
        CheckBoxColumn,
        PathColumn,
        CapacityColumn,
        LoginColumn,
        PasswordColumn,
        ArchiveSpaceColumn,
        ColumnCount
    };

    class ArchiveSpaceItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        ArchiveSpaceItemDelegate(QObject *parent = NULL): base_type(parent) {}

        virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
            return new QnStorageSpaceSlider(parent);
        }

        virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override {
            QnStorageSpaceSlider *slider = dynamic_cast<QnStorageSpaceSlider *>(editor);
            if(!slider) {
                base_type::setEditorData(editor, index);
                return;
            }

            qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
            qint64 videoSpace = totalSpace - index.data(ReservedSpaceRole).toLongLong();

            slider->setRange(0, totalSpace / bytesInMiB);
            slider->setValue(videoSpace / bytesInMiB);
        }

        virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
            QnStorageSpaceSlider *slider = dynamic_cast<QnStorageSpaceSlider *>(editor);
            if(!slider) {
                base_type::setModelData(editor, model, index);
                return;
            }

            qint64 totalSpace = index.data(TotalSpaceRole).toLongLong();
            qint64 videoSpace = slider->value() * bytesInMiB;
            model->setData(index, totalSpace - videoSpace, ReservedSpaceRole);
        }
    };

} // anonymous namespace


QnServerSettingsDialog::QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::ServerSettingsDialog),
    m_server(server),
    m_hasStorageChanges(false),
    m_maxCamerasAdjusted(false),
    m_rebuildState(RebuildState::Invalid)
{
    ui->setupUi(this);

    ui->storagesTable->resizeColumnsToContents();
    ui->storagesTable->horizontalHeader()->setSectionsClickable(false);
    ui->storagesTable->horizontalHeader()->setStretchLastSection(false);
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(CheckBoxColumn, QHeaderView::Fixed);
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(PathColumn, QHeaderView::Stretch);
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(CapacityColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(LoginColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(PasswordColumn, QHeaderView::ResizeToContents);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    ui->storagesTable->horizontalHeader()->setSectionResizeMode(ArchiveSpaceColumn, QHeaderView::ResizeToContents);
    ui->storagesTable->setItemDelegateForColumn(ArchiveSpaceColumn, new ArchiveSpaceItemDelegate(this));
#else
    ui->storagesTable->setColumnCount(ColumnCount - 1);
#endif

    setWarningStyle(ui->failoverWarningLabel);

    m_removeAction = new QAction(tr("Remove Storage"), this);

    QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    signalizer->setEventType(QEvent::ContextMenu);
    ui->storagesTable->installEventFilter(signalizer);
    connect(signalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_storagesTable_contextMenuEvent(QObject *, QEvent *)));
    connect(m_server, SIGNAL(statusChanged(QnResourcePtr)), this, SLOT(at_updateRebuildInfo()));
    connect(m_server, SIGNAL(serverIfFound(QnMediaServerResourcePtr, QString, QString)), this, SLOT(at_updateRebuildInfo()));

    /* Set up context help. */
    setHelpTopic(ui->nameLabel,           ui->nameLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->nameLabel,           ui->maxCamerasSpinBox,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->ipAddressLabel,      ui->ipAddressLineEdit,              Qn::ServerSettings_General_Help);
    setHelpTopic(ui->portLabel,           ui->portLineEdit,                   Qn::ServerSettings_General_Help);
    setHelpTopic(ui->storagesGroupBox,                                        Qn::ServerSettings_Storages_Help);
    setHelpTopic(ui->rebuildGroupBox,                                         Qn::ServerSettings_ArchiveRestoring_Help);

    connect(ui->storagesTable,          SIGNAL(cellChanged(int, int)),  this,   SLOT(at_storagesTable_cellChanged(int, int)));
    connect(ui->pingButton,             SIGNAL(clicked()),              this,   SLOT(at_pingButton_clicked()));

    connect(ui->rebuildStartButton,     SIGNAL(clicked()),              this,   SLOT(at_rebuildButton_clicked()));
    connect(ui->rebuildStopButton,      SIGNAL(clicked()),              this,   SLOT(at_rebuildButton_clicked()));

    connect(ui->failoverCheckBox,       &QCheckBox::stateChanged,       this,   [this] {
        ui->maxCamerasWidget->setEnabled(ui->failoverCheckBox->isChecked());
        updateFailoverLabel();
    });
    connect(ui->maxCamerasSpinBox,      QnSpinboxIntValueChanged,       this,   [this] {
        m_maxCamerasAdjusted = true;
        updateFailoverLabel();
    });

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
    int row = dataRowCount();
    ui->storagesTable->insertRow(row);

    QTableWidgetItem *checkBoxItem = new QTableWidgetItem();
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    if (item.isWritable)
        flags |= Qt::ItemIsEnabled;
    checkBoxItem->setFlags(flags);
    checkBoxItem->setCheckState(item.isUsedForWriting ? Qt::Checked : Qt::Unchecked);
    checkBoxItem->setData(StorageIdRole, QVariant::fromValue<QnUuid>(item.storageId));
    checkBoxItem->setData(ExternalRole, item.isExternal);

    QTableWidgetItem *pathItem = new QTableWidgetItem();
    pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    pathItem->setData(Qt::DisplayRole, QnAbstractStorageResource::urlToPath(item.url));

    QTableWidgetItem *capacityItem = new QTableWidgetItem();
    capacityItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    capacityItem->setData(Qt::DisplayRole, item.totalSpace == -1 ? tr("Not available") : QnStorageSpaceSlider::formatSize(item.totalSpace));

    QUrl url(item.url);
    QTableWidgetItem *loginItem = new QTableWidgetItem();
    loginItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    loginItem->setData(Qt::DisplayRole, url.userName());

    QTableWidgetItem *passwordItem = new QTableWidgetItem();
    passwordItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    passwordItem->setData(Qt::DisplayRole, url.password());

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
    ui->storagesTable->setItem(row, LoginColumn, loginItem);
    ui->storagesTable->setItem(row, PasswordColumn, passwordItem);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    ui->storagesTable->setItem(row, ArchiveSpaceColumn, archiveSpaceItem);
    ui->storagesTable->openPersistentEditor(archiveSpaceItem);
#endif
}

void QnServerSettingsDialog::setTableItems(const QList<QnStorageSpaceData> &items) {
    QString bottomLabelText = this->bottomLabelText();

    ui->storagesTable->setRowCount(0);
    foreach(const QnStorageSpaceData &item, items)
        addTableItem(item);

    setBottomLabelText(bottomLabelText);
}

QnStorageSpaceData QnServerSettingsDialog::tableItem(int row) const {
    QnStorageSpaceData result;
    if(row < 0 || row >= dataRowCount())
        return result;

    QTableWidgetItem *checkBoxItem = ui->storagesTable->item(row, CheckBoxColumn);
    QTableWidgetItem *pathItem = ui->storagesTable->item(row, PathColumn);
    QTableWidgetItem *capacityItem = ui->storagesTable->item(row, CapacityColumn);
#ifdef QN_SHOW_ARCHIVE_SPACE_COLUMN
    QTableWidgetItem *archiveSpaceItem = ui->storagesTable->item(row, ArchiveSpaceColumn);
#else
    QTableWidgetItem *archiveSpaceItem = capacityItem;
#endif

    result.isWritable = checkBoxItem->flags() & Qt::ItemIsEnabled;
    result.isUsedForWriting = checkBoxItem->checkState() == Qt::Checked;
    result.storageId = checkBoxItem->data(StorageIdRole).value<QnUuid>();
    result.isExternal = qvariant_cast<bool>(checkBoxItem->data(ExternalRole), true);

    QString login = ui->storagesTable->item(row, LoginColumn)->text();
    QString password = ui->storagesTable->item(row, PasswordColumn)->text();
    if (login.isEmpty())
        result.url = pathItem->text();
    else {
        QUrl url = QString(lit("file:///%1")).arg(pathItem->text());
        url.setUserName(login);
        url.setPassword(password);
        result.url = url.toString();
    }

    result.totalSpace = archiveSpaceItem->data(TotalSpaceRole).toLongLong();
    result.freeSpace = archiveSpaceItem->data(FreeSpaceRole).toLongLong();
    result.reservedSpace = archiveSpaceItem->data(ReservedSpaceRole).toLongLong();

    return result;
}

QList<QnStorageSpaceData> QnServerSettingsDialog::tableItems() const {
    QList<QnStorageSpaceData> result;
    for(int row = 0; row < dataRowCount(); row++)
        result.push_back(tableItem(row));
    return result;
}

void QnServerSettingsDialog::updateFromResources() 
{
    m_server->apiConnection()->getStorageSpaceAsync(this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
    updateRebuildUi(RebuildState::Invalid);

    if (m_server->getStatus() == Qn::Online)
        sendNextArchiveRequest();

    setTableItems(QList<QnStorageSpaceData>());
    setBottomLabelText(tr("Loading..."));

    //bool edge = QnMediaServerResource::isEdgeServer(m_server);
    ui->nameLineEdit->setText(m_server->getName());
    //ui->nameLineEdit->setEnabled(!edge);
    ui->maxCamerasSpinBox->setValue(m_server->getMaxCameras());
    ui->failoverCheckBox->setChecked(m_server->isRedundancy());
    //ui->failoverCheckBox->setEnabled(!edge);
    ui->maxCamerasWidget->setEnabled(m_server->isRedundancy());

    int maxCameras = (m_server->getServerFlags() & Qn::SF_Edge) ? 1 : 128;
    ui->maxCamerasSpinBox->setMaximum(maxCameras);

    ui->ipAddressLineEdit->setText(QUrl(m_server->getUrl()).host());
    ui->portLineEdit->setText(QString::number(QUrl(m_server->getUrl()).port()));

    m_hasStorageChanges = false;
    m_maxCamerasAdjusted = false;

    updateFailoverLabel();
}

void QnServerSettingsDialog::submitToResources() {
    if(m_hasStorageChanges) {
        QnServerStorageStateHash serverStorageStates = qnSettings->serverStorageStates();

        QnAbstractStorageResourceList storages;
        foreach(const QnStorageSpaceData &item, tableItems()) {
            if(!item.isUsedForWriting && item.storageId.isNull()) {
                serverStorageStates.insert(QnServerStorageKey(m_server->getId(), item.url), item.reservedSpace);
                continue;
            }

            QnAbstractStorageResourcePtr storage(new QnAbstractStorageResource());
            if (!item.storageId.isNull())
                storage->setId(item.storageId);
            QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(lit("Storage"));
            if (resType)
                storage->setTypeId(resType->getId());
            storage->setName(QnUuid::createUuid().toString());
            storage->setParentId(m_server->getId());
            storage->setUrl(item.url);
            storage->setSpaceLimit(item.reservedSpace); //client does not change space limit anymore
            storage->setUsedForWriting(item.isUsedForWriting);

            storages.push_back(storage);
        }
        //m_server->setStorages(storages);

        qnSettings->setServerStorageStates(serverStorageStates);
    }

    m_server->setName(ui->nameLineEdit->text());
    m_server->setMaxCameras(ui->maxCamerasSpinBox->value());
    m_server->setRedundancy(ui->failoverCheckBox->isChecked());
}

void QnServerSettingsDialog::setBottomLabelText(const QString &text) {
    bool hasLabel = dataRowCount() != ui->storagesTable->rowCount();

    if(!text.isEmpty()) {
        if(!hasLabel) {
            QLabel *tableBottomLabel = new QLabel();
            tableBottomLabel->setAlignment(Qt::AlignCenter);
            connect(tableBottomLabel, SIGNAL(linkActivated(const QString &)), this, SLOT(at_tableBottomLabel_linkActivated()));

            int row = ui->storagesTable->rowCount();
            ui->storagesTable->insertRow(row);
            ui->storagesTable->setSpan(row, 0, 1, ColumnCount);
            ui->storagesTable->setCellWidget(row, 0, tableBottomLabel);

            m_tableBottomLabel = tableBottomLabel;
        }

        m_tableBottomLabel.data()->setText(text);
    } else {
        if(!hasLabel)
            return;

        ui->storagesTable->setRowCount(ui->storagesTable->rowCount() - 1);
    }
}

QString QnServerSettingsDialog::bottomLabelText() const {
    bool hasLabel = dataRowCount() != ui->storagesTable->rowCount();

    if(hasLabel && m_tableBottomLabel) {
        return m_tableBottomLabel.data()->text();
    } else {
        return QString();
    }
}

int QnServerSettingsDialog::dataRowCount() const {
    int rowCount = ui->storagesTable->rowCount();
    if(rowCount == 0)
        return rowCount;

    if(!m_tableBottomLabel)
        return rowCount;

    /* Label could not yet be deleted even if the corresponding row is already removed,
     * so we have to check that it's there. */
    QWidget *cellWidget = ui->storagesTable->cellWidget(rowCount - 1, 0);
    if(cellWidget)
        return rowCount - 1;

    return rowCount;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerSettingsDialog::at_tableBottomLabel_linkActivated() {
    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(m_storageProtocols);
    if(!dialog->exec())
        return;

    QnStorageSpaceData item = dialog->storage();
    if(!item.storageId.isNull())
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

void QnServerSettingsDialog::at_storagesTable_contextMenuEvent(QObject *, QEvent *) {
    int row = ui->storagesTable->currentRow();
    QnStorageSpaceData item = tableItem(row);
    if(item.url.isEmpty() || !item.isExternal)
        return;

    QScopedPointer<QMenu> menu(new QMenu(this));
    menu->addAction(m_removeAction);

    QAction *action = menu->exec(QCursor::pos());
    if(action == m_removeAction) {
        ui->storagesTable->removeRow(row);
        m_hasStorageChanges = true;
    }
}

void QnServerSettingsDialog::at_rebuildButton_clicked()
{
    RebuildAction action;
    RebuildState newState = RebuildState::Invalid;
    if (m_rebuildState == RebuildState::InProgress) {
        action = RebuildAction_Cancel;
        newState = RebuildState::Stopping;
    } else {
        action = RebuildAction_Start;
        newState = RebuildState::Starting;
    }

    if (action == RebuildAction_Start)
    {
        int button = QMessageBox::warning(
            this,
            tr("Warning"),
            tr("You are about to launch the archive re-synchronization routine. ATTENTION! Your hard disk usage will be increased during re-synchronization process! "
            "Depending on the total size of archive it can take several hours. "
            "This process is only necessary if your archive folders have been moved, renamed or replaced. You can cancel rebuild operation at any moment without loosing data. Continue?"),
            QMessageBox::Yes | QMessageBox::No
            );
        if(button == QMessageBox::No)
            return;
    }

    m_server->apiConnection()->doRebuildArchiveAsync (action, this, SLOT(at_archiveRebuildReply(int, const QnRebuildArchiveReply &, int)));
    updateRebuildUi(newState);
}

void QnServerSettingsDialog::at_updateRebuildInfo()
{
    if (m_server->getStatus() == Qn::Online)
        sendNextArchiveRequest();
    else
        updateRebuildUi(RebuildState::Invalid);
}

void QnServerSettingsDialog::sendNextArchiveRequest()
{
    m_server->apiConnection()->doRebuildArchiveAsync (RebuildAction_ShowProgress, this, SLOT(at_archiveRebuildReply(int, const QnRebuildArchiveReply &, int)));
}

void QnServerSettingsDialog::updateRebuildUi(RebuildState newState, int progress) {
     RebuildState oldState = m_rebuildState;
     m_rebuildState = newState;

     ui->rebuildGroupBox->setEnabled(newState != RebuildState::Invalid);
     if (progress >= 0)
        ui->rebuildProgressBar->setValue(progress);
     ui->rebuildStartButton->setEnabled(newState == RebuildState::Ready);
     ui->rebuildStopButton->setEnabled(newState == RebuildState::InProgress);

     if (oldState == RebuildState::InProgress && newState == RebuildState::Ready)
         QMessageBox::information(this,
         tr("Finished"),
         tr("Rebuilding archive index is completed."));

     ui->stackedWidget->setCurrentIndex(newState == RebuildState::InProgress 
         ? ui->stackedWidget->indexOf(ui->rebuildProgressPage)
         : ui->stackedWidget->indexOf(ui->rebuildPreparePage));
}

void QnServerSettingsDialog::updateFailoverLabel() {

    auto getErrorText = [this] {
        if (qnResPool->getResources<QnMediaServerResource>().size() < 2)
            return tr("At least two servers are required for this feature.");

        if (qnResPool->getAllCameras(m_server, true).size() > ui->maxCamerasSpinBox->value())
            return tr("This server already has more than max cameras");

        if (!m_server->isRedundancy() && !m_maxCamerasAdjusted)
            return tr("To avoid malfunction adjust max number of cameras");

        return QString();
    };

    QString error;
    if (ui->failoverCheckBox->isChecked())
        error = getErrorText();

    ui->failoverWarningLabel->setText(error);
}


void QnServerSettingsDialog::at_archiveRebuildReply(int status, const QnRebuildArchiveReply& reply, int handle)
{
    Q_UNUSED(handle)
    RebuildState state = RebuildState::Invalid;

    if (status == 0) {
        switch (reply.state()) {
        case QnRebuildArchiveReply::Started:
            state = RebuildState::InProgress;
            break;
        case QnRebuildArchiveReply::Stopped:
            state = RebuildState::Ready;
            break;
        default:
            break;
        }
    }
    updateRebuildUi(state, reply.progress());

    if (reply.state() == QnRebuildArchiveReply::Started)
        QTimer::singleShot(500, this, SLOT(sendNextArchiveRequest()));
}

void QnServerSettingsDialog::at_pingButton_clicked() {
    menu()->trigger(Qn::PingAction, QnActionParameters(m_server));
}

void QnServerSettingsDialog::at_replyReceived(int status, const QnStorageSpaceReply &reply, int) {
    if(status != 0) {
        setBottomLabelText(tr("Could not load storages from server."));
        return;
    }

    QnServerStorageStateHash serverStorageStates = qnSettings->serverStorageStates();

    QList<QnStorageSpaceData> items = reply.storages;
    for(int i = 0; i < items.size(); i++) {
        QnStorageSpaceData &item = items[i];

        if(item.reservedSpace == -1)
            item.reservedSpace = serverStorageStates.value(QnServerStorageKey(m_server->getId(), item.url) , -1);
    }

    struct StorageSpaceDataLess {
        bool operator()(const QnStorageSpaceData &l, const QnStorageSpaceData &r) {
            bool lLocal = l.url.contains(lit("://"));
            bool rLocal = r.url.contains(lit("://"));

            if(lLocal != rLocal)
                return lLocal;

            return l.url < r.url;
        }
    };
    qSort(items.begin(), items.end(), StorageSpaceDataLess());

    setTableItems(items);

    m_storageProtocols = reply.storageProtocols;
    if(m_storageProtocols.isEmpty()) {
        setBottomLabelText(QString());
    } else {
        setBottomLabelText(tr("<a href='1'>Add external Storage...</a>"));
    }

    m_hasStorageChanges = false;
}
