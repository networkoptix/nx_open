#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"

#include <api/model/storage_space_reply.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>

#include <camera/camera_data_manager.h>

#include <core/resource/client_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <server/server_storage_manager.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/dialogs/backup_schedule_dialog.h>
#include <ui/models/storage_list_model.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <utils/common/qtimespan.h>

namespace {
    static const int COLUMN_SPACING = 8;
    static const int MIN_COL_WIDTH = 16;


    class QnStoragesPoolFilterModel: public QSortFilterProxyModel {
    public:
        QnStoragesPoolFilterModel(bool isMainPool, QObject *parent = nullptr)
            : QSortFilterProxyModel(parent)
            , m_isMainPool(isMainPool)
        {}
    protected:
        virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
            QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
            if(!index.isValid())
                return true;

            QnStorageModelInfo storage = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();
            return storage.isBackup != m_isMainPool;
        }

    private:
        bool m_isMainPool;
    };


    const qint64 minDeltaForMessageMs = 1000ll * 3600 * 24;

} // anonymous namespace

QnStorageConfigWidget::StoragePool::StoragePool() 
    : rebuildCancelled(false)
{}


class QnStorageTableItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnStorageTableItemDelegate(QObject *parent = NULL): base_type(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override
    {
        QSize result = base_type::sizeHint(option, index);
        result.setWidth(result.width() + COLUMN_SPACING);
        return result;
    }
};

QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::StorageConfigWidget())
    , m_server()
    , m_model(new QnStorageListModel())
    , m_mainPool()
    , m_backupPool()
    , m_backupSchedule()
    , m_backupCancelled(false)
    , m_updating(false)
    , m_backupTypeLastIndex(0)
{
    ui->setupUi(this);
 
    ui->comboBoxBackupType->addItem(tr("By schedule"), Qn::Backup_Schedule);
    ui->comboBoxBackupType->addItem(tr("In realtime"), Qn::Backup_RealTime);
    ui->comboBoxBackupType->addItem(tr("On demand"), Qn::Backup_Manual);

    setupGrid(ui->mainStoragesTable, true);
    setupGrid(ui->backupStoragesTable, false);

    connect(ui->comboBoxBackupType,         QnComboboxCurrentIndexChanged, this,  &QnStorageConfigWidget::at_backupTypeComboBoxChange);

    connect(ui->addExtStorageToMainBtn,     &QPushButton::clicked, this, [this]() {at_addExtStorage(true); });
    connect(ui->addExtStorageToBackupBtn,   &QPushButton::clicked, this, [this]() {at_addExtStorage(false); });

    connect(ui->rebuildMainButton,          &QPushButton::clicked, this, [this] { startRebuid(true); });
    connect(ui->rebuildBackupButton,        &QPushButton::clicked, this, [this] { startRebuid(false); });

    connect(ui->rebuildMainWidget,          &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(true); });
    connect(ui->rebuildBackupWidget,        &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(false); });

    connect(ui->pushButtonSchedule,         &QPushButton::clicked, this, &QnStorageConfigWidget::at_openBackupSchedule_clicked);
    connect(ui->camerasToBackupButton,      &QPushButton::clicked,  this, [this]{
        menu()->trigger(Qn::OpenBackupCamerasAction);
        updateBackupInfo();
    });

    connect(ui->backupStartButton,          &QPushButton::clicked, this, &QnStorageConfigWidget::startBackup);
    connect(ui->backupStopButton,           &QPushButton::clicked, this, &QnStorageConfigWidget::cancelBackup);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildStatusChanged,this, &QnStorageConfigWidget::at_serverRebuildStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupStatusChanged, this, &QnStorageConfigWidget::at_serverBackupStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished, this, &QnStorageConfigWidget::at_serverRebuildArchiveFinished);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupFinished, this, &QnStorageConfigWidget::at_serverBackupFinished);

    connect(this, &QnAbstractPreferencesWidget::hasChangesChanged, this, [this]() {
        if (m_updating)
            return;

        updateBackupInfo();
        updateRebuildInfo();
    });

    at_backupTypeComboBoxChange(ui->comboBoxBackupType->currentIndex());

    retranslateUi();
}

void QnStorageConfigWidget::retranslateUi() {
    ui->camerasToBackupButton->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Devices to Backup..."),
        tr("Cameras to Backup...")
        ));
}


QnStorageConfigWidget::~QnStorageConfigWidget()
{}

void QnStorageConfigWidget::setReadOnlyInternal( bool readOnly ) {
    m_model->setReadOnly(readOnly);
    //TODO: #GDM #Safe Mode
}

void QnStorageConfigWidget::showEvent( QShowEvent *event ) {
    base_type::showEvent(event);
    if (m_server)
        qnServerStorageManager->checkBackupStatus(m_server);
}

bool QnStorageConfigWidget::hasChanges() const {
    if (isReadOnly())
        return false;

    if (hasStoragesChanges(m_model->storages()))
        return true;

    return (m_server->getBackupSchedule() != m_backupSchedule);
}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain) {
    if (!m_server || isReadOnly())
        return;

    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));    
    dialog->setProtocols(qnServerStorageManager->protocols(m_server));
    if(!dialog->exec())
        return;

    QnStorageModelInfo item = dialog->storage();
    /* Check if somebody have added this storage right now */
    if(item.id.isNull())
    {
        item.id = QnStorageResource::fillID(m_server->getId(), item.url);
        item.isBackup = !addToMain;
        item.isUsed = true;
    }

    m_model->addStorage(item);  /// Adds or updates storage model data
    updateColumnWidth();

    emit hasChangesChanged();
}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, bool isMainPool)
{
    tableView->setItemDelegate(new QnStorageTableItemDelegate(this));
    setWarningStyle(ui->storagesWarningLabel);
    ui->storagesWarningLabel->hide();
    
    QnStoragesPoolFilterModel* filterModel = new QnStoragesPoolFilterModel(isMainPool, this);
    filterModel->setSourceModel(m_model.data());
    tableView->setModel(filterModel);
    
    if (!isMainPool)
    {
        // Hides table when data model is empty
        const auto onCountChanged = [this, tableView]()
        {
            const auto model = tableView->model();
            const bool shouldBeVisible = (model && model->rowCount());
            const bool currentVisible = tableView->isVisible();
            if (currentVisible != shouldBeVisible)
            {
                tableView->setVisible(shouldBeVisible);
                ui->backupControls->setVisible(shouldBeVisible);
            }
        };

        tableView->setVisible(false);
        ui->backupControls->setVisible(false);

        connect(tableView->model(), &QAbstractItemModel::rowsRemoved, this, onCountChanged);
        connect(tableView->model(), &QAbstractItemModel::rowsInserted, this, onCountChanged);
        connect(tableView->model(), &QAbstractItemModel::modelReset, this, onCountChanged);
    }
    
    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->horizontalHeader()->setSectionResizeMode(QnStorageListModel::UrlColumn, QHeaderView::Stretch);

    
    connect(tableView,         &QTableView::clicked,               this,   &QnStorageConfigWidget::at_eventsGrid_clicked);
    connect(filterModel, &QnStorageListModel::dataChanged, this, 
        [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
            QN_UNUSED(topLeft, bottomRight);
            if (!m_updating && roles.contains(Qt::CheckStateRole))
                emit hasChangesChanged();
    });

    tableView->setMouseTracking(true);
}

void QnStorageConfigWidget::at_backupTypeComboBoxChange(int index) 
{
    const auto currentBackupType = static_cast<Qn::BackupType>(ui->comboBoxBackupType->itemData(index).toInt());
    
    m_backupSchedule.backupType = currentBackupType;
    ui->pushButtonSchedule->setEnabled(currentBackupType == Qn::Backup_Schedule);
    ui->backupTimeLabel->setVisible(currentBackupType != Qn::Backup_RealTime);

    if (index != m_backupTypeLastIndex)
    {
        if (currentBackupType == Qn::Backup_RealTime)
        {
            QMessageBox::warning(this, tr("Warning")
                , tr("Previous footage will not be backed up!"), QMessageBox::Ok);
        }

        m_backupTypeLastIndex = index;
    }

    emit hasChangesChanged();
}

void QnStorageConfigWidget::loadDataToUi() {
    if (!m_server)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    loadStoragesFromResources();
    m_backupSchedule = m_server->getBackupSchedule();
    m_backupTypeLastIndex = ui->comboBoxBackupType->findData(m_backupSchedule.backupType);
    ui->comboBoxBackupType->setCurrentIndex(m_backupTypeLastIndex);    

    updateRebuildInfo();
    updateBackupInfo();

}

void QnStorageConfigWidget::loadStoragesFromResources() {
    Q_ASSERT_X(m_server, Q_FUNC_INFO, "Server must exist here");

    QnStorageModelInfoList storages;       
    for (const QnStorageResourcePtr &storage: m_server->getStorages())
        storages.append(QnStorageModelInfo(storage));
    m_model->setStorages(storages);

    updateColumnWidth();
}

void QnStorageConfigWidget::at_eventsGrid_clicked(const QModelIndex& index)
{
    if (!m_server || isReadOnly())
        return;

    QnStorageModelInfo record = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

    if (index.column() == QnStorageListModel::ChangeGroupActionColumn)
    {
        if (!m_model->canMoveStorage(record))
            return;

        record.isBackup = !record.isBackup;
        m_model->updateStorage(record);
        updateColumnWidth();
    }
    else if (index.column() == QnStorageListModel::RemoveActionColumn)
    {
        if (record.isExternal)
            m_model->removeStorage(record);
        updateColumnWidth();
    }

    emit hasChangesChanged();
}

void QnStorageConfigWidget::updateColumnWidth() {
    for (int i = 0; i < QnStorageListModel::ColumnCount; ++i) {

        /* Stretch url column */
        if (i == QnStorageListModel::UrlColumn)
            continue;

        int width = getColWidth(i);
        ui->mainStoragesTable->setColumnWidth(i, width + COLUMN_SPACING);
        ui->backupStoragesTable->setColumnWidth(i, width + COLUMN_SPACING);
    }
}

int QnStorageConfigWidget::getColWidth(int col)
{
    int result = MIN_COL_WIDTH;
    QFont f = ui->mainStoragesTable->font();
    QFontMetrics fm(f);
    for (int i = 0; i < m_model->rowCount(QModelIndex()); ++i)
    {
        QModelIndex index = m_model->index(i, col);
        int w = fm.width(m_model->data(index, Qn::TextWidthDataRole).toString());
        result = qMax(result, w);
    }

    return result;
}

void QnStorageConfigWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server)
        return;

    m_server = server;
}

void QnStorageConfigWidget::updateRebuildInfo() {
    for (int i = 0; i < static_cast<int>(QnServerStoragesPool::Count); ++i) {
        QnServerStoragesPool pool = static_cast<QnServerStoragesPool>(i);
        updateRebuildUi(pool, qnServerStorageManager->rebuildStatus(m_server, pool));
    }
}

void QnStorageConfigWidget::updateBackupInfo() {
    updateBackupUi(qnServerStorageManager->backupStatus(m_server));
}

void QnStorageConfigWidget::applyStoragesChanges(QnStorageResourceList& result, const QnStorageModelInfoList &storages) const {
    for(const auto& storageData: storages)
    {
        QnStorageResourcePtr storage = m_server->getStorageByUrl(storageData.url);
        if (storage) {
            if (storageData.isUsed != storage->isUsedForWriting() || storageData.isBackup != storage->isBackup()) 
            {
                storage->setUsedForWriting(storageData.isUsed);
                storage->setBackup(storageData.isBackup);
                result << storage;
            }
        }
        else {
            QnClientStorageResourcePtr storage = QnClientStorageResource::newStorage(m_server, storageData.url);
            Q_ASSERT_X(storage->getId() == storageData.id, Q_FUNC_INFO, "Id's must be equal");

            storage->setUsedForWriting(storageData.isUsed);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(storageData.isBackup);

            qnResPool->addResource(storage);
            result << storage;
        }
    }
}

bool QnStorageConfigWidget::hasStoragesChanges( const QnStorageModelInfoList &storages) const {
    if (!m_server)
        return false;

    for(const auto& storageData: storages) {
        /* New storage was added. */
        QnStorageResourcePtr storage = qnResPool->getResourceById<QnStorageResource>(storageData.id);
        if (!storage || storage->getParentId() != m_server->getId())
            return true;

        /* Storage was modified. */
        if (storageData.isUsed != storage->isUsedForWriting() || storageData.isBackup != storage->isBackup())
            return true;
    }

    /* Storage was removed. */
    auto existingStorages = m_server->getStorages();
    return storages.size() != existingStorages.size();
}


void QnStorageConfigWidget::applyChanges()
{
    if (isReadOnly())
        return;

    QnStorageResourceList storagesToUpdate;
    ec2::ApiIdDataList storagesToRemove;

    applyStoragesChanges(storagesToUpdate, m_model->storages());
    
    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_model->storages())
        newIdList << storageData.id;

    for (const auto& storage: m_server->getStorages()) {
        if (!newIdList.contains(storage->getId())) {
            storagesToRemove.push_back(storage->getId());
            qnResPool->removeResource(storage);
        }
    }

    if (!storagesToUpdate.empty())
        qnServerStorageManager->saveStorages(storagesToUpdate);


    if (!storagesToRemove.empty())
        qnServerStorageManager->deleteStorages(storagesToRemove);
    
    if (m_backupSchedule != m_server->getBackupSchedule()) {
        qnResourcesChangesManager->saveServer(m_server, [this](const QnMediaServerResourcePtr &server) {
            server->setBackupSchedule(m_backupSchedule);
        });
    }

    emit hasChangesChanged();
}

void QnStorageConfigWidget::startRebuid(bool isMain) {
    if (!m_server)
        return;

    int warnResult = QMessageBox::warning(
        this,
        tr("Warning"),
        tr("You are about to launch the archive re-synchronization routine.") + L'\n' 
        + tr("ATTENTION! Your hard disk usage will be increased during re-synchronization process! Depending on the total size of archive it can take several hours.") + L'\n' 
        + tr("This process is only necessary if your archive folders have been moved, renamed or replaced. You can cancel rebuild operation at any moment without loosing data.") + L'\n' 
        + tr("Are you sure you want to continue?"),
        QMessageBox::Ok | QMessageBox::Cancel
        );
    if(warnResult != QMessageBox::Ok)
        return;
    
    if (!qnServerStorageManager->rebuildServerStorages(m_server, isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
        return;
   
    if (isMain)
        ui->rebuildMainButton->setEnabled(false);
    else
        ui->rebuildBackupButton->setEnabled(false);


    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::cancelRebuild(bool isMain) {
    if (!qnServerStorageManager->cancelServerStoragesRebuild(m_server, isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
        return;

    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    storagePool.rebuildCancelled = true;
}

void QnStorageConfigWidget::startBackup() {
    if (!m_server)
        return;

    if (qnServerStorageManager->backupServerStorages(m_server)) {
        ui->backupStartButton->setEnabled(false);
        m_backupCancelled = false;
    }
}

void QnStorageConfigWidget::cancelBackup() {
    if (!m_server)
        return;

    if (qnServerStorageManager->cancelBackupServerStorages(m_server)) {
        ui->backupStopButton->setEnabled(false);
        m_backupCancelled = true;
    }
}

void QnStorageConfigWidget::at_openBackupSchedule_clicked() {
    auto scheduleDialog = new QnBackupScheduleDialog(this);
    scheduleDialog->updateFromSettings(m_backupSchedule);
    if (scheduleDialog->exec())
        if (!isReadOnly())
            scheduleDialog->submitToSettings(m_backupSchedule);
}

bool QnStorageConfigWidget::canStartBackup(const QnBackupStatusData& data, QString *info) {

    using boost::algorithm::any_of;

    auto error = [info](const QString &error) -> bool {
        if (info)
            *info = error;
        return false;
    };

    if (data.state != Qn::BackupState_None) 
        return error(tr("Backup is already in progress."));

    if (m_backupSchedule.backupType == Qn::Backup_RealTime)
        return error(tr("In Realtime mode all data is backed up on continuously"));

    if (!any_of(m_model->storages(), [](const QnStorageModelInfo &storage){
        return storage.isWritable && storage.isUsed && storage.isBackup;
    }))
        return error(tr("Select at least one backup storage."));
   
    if (!any_of(qnCameraHistoryPool->getServerFootageCameras(m_server), [](const QnVirtualCameraResourcePtr &camera){
        return camera->getActualBackupQualities() != Qn::CameraBackup_Disabled;
    }))
        return error(tr("Select at least one camera with archive to backup"));

    if (hasChanges())
        return error(tr("Apply changes before starting backup."));

    return true;
}

QString QnStorageConfigWidget::backupPositionToString( qint64 backupTimeMs ) {

    const QDateTime backupDateTime = QDateTime::fromMSecsSinceEpoch(backupTimeMs);
    QString result = lit("%1 %2").arg(backupDateTime.date().toString()).arg(backupDateTime.time().toString(Qt::SystemLocaleLongDate));

    qint64 deltaMs = qnSyncTime->currentDateTime().toMSecsSinceEpoch() - backupTimeMs;
    if (deltaMs > minDeltaForMessageMs) {
        QTimeSpan span(deltaMs);
        span.normalize();
        QString deltaStr = tr("(%1 before now)").arg(span.toApproximateString());
        return lit("%1 %2").arg(result).arg(deltaStr);
    } 

    return result;
}


void QnStorageConfigWidget::updateBackupUi(const QnBackupStatusData& reply)
{
    QString status;

    if (reply.progress >= 0)
        ui->progressBarBackup->setValue(reply.progress * 100 + 0.5);

    QString backupInfo;
    bool canStartBackup = this->canStartBackup(reply, &backupInfo);
    ui->backupWarningLabel->setText(backupInfo);

    //TODO: #GDM discuss texts
    QString backedUpTo = reply.backupTimeMs > 0
        ? tr("Archive backup is created up to: %1.").arg(backupPositionToString(reply.backupTimeMs))
        : tr("Backup was never started.");
    ui->backupTimeLabel->setText(backedUpTo);

    ui->backupStartButton->setEnabled(canStartBackup);
    ui->backupStopButton->setEnabled(reply.state == Qn::BackupState_InProgress);
    ui->stackedWidgetBackupInfo->setCurrentWidget(reply.state == Qn::BackupState_InProgress ? ui->backupProgressPage : ui->backupPreparePage);
}

void QnStorageConfigWidget::updateRebuildUi(QnServerStoragesPool pool, const QnStorageScanData& reply)
{
    using boost::algorithm::any_of;

    bool isMainPool = pool == QnServerStoragesPool::Main;

    bool canStartRebuild = 
            reply.state == Qn::RebuildState_None
        &&  !hasChanges()   
        &&  any_of(m_model->storages(), [isMainPool](const QnStorageModelInfo &info) {
                return info.isWritable 
                    && info.isBackup != isMainPool;
            });

    if (isMainPool) {
        ui->rebuildMainWidget->loadData(reply);
        ui->rebuildMainButton->setEnabled(canStartRebuild);
    }
    else {
        ui->rebuildBackupWidget->loadData(reply);
        ui->rebuildBackupButton->setEnabled(canStartRebuild);
    }   
}

void QnStorageConfigWidget::at_serverRebuildStatusChanged( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageScanData &status ) {
    if (server != m_server)
        return;

    updateRebuildUi(pool, status);
}

void QnStorageConfigWidget::at_serverBackupStatusChanged( const QnMediaServerResourcePtr &server, const QnBackupStatusData &status ) {
    if (server != m_server)
        return;

    updateBackupUi(status);
}

void QnStorageConfigWidget::at_serverRebuildArchiveFinished( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool ) {
    if (server != m_server)
        return;

    bool isMain = (pool == QnServerStoragesPool::Main);
    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    if (!storagePool.rebuildCancelled)
        QMessageBox::information(this,
            tr("Finished"),
            tr("Rebuilding archive index is completed."));
    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::at_serverBackupFinished( const QnMediaServerResourcePtr &server ) {
    if (server != m_server)
        return;

    if (!m_backupCancelled)
        QMessageBox::information(this,
            tr("Finished"),
            tr("Archive backup is completed."));
    m_backupCancelled = false;
}
