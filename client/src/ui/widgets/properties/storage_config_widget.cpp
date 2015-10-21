#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
#include <api/model/storage_space_reply.h>

#include <camera/camera_data_manager.h>

#include <core/resource/client_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/ui_resource_name.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/dialogs/backup_schedule_dialog.h>
#include <ui/dialogs/backup_settings_dialog.h>
#include <ui/models/storage_list_model.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>


namespace {
    //setting free space to zero, since now client does not change this value, so it must keep current value
    const qint64 defaultReservedSpace = 0;  //5ll * 1024ll * 1024ll * 1024ll;

    static const int COLUMN_SPACING = 8;
    static const int MIN_COL_WIDTH = 16;

    const int updateRebuildStatusDelayMs = 500;
    const int updateBackupStatusDelayMs = 500;

} // anonymous namespace


QnStorageConfigWidget::StoragePool::StoragePool() 
    : storageSpaceHandle(0)
    , rebuildHandle(0)
    , model(new QnStorageListModel())
{

}


class QnCustomItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnCustomItemDelegate(QObject *parent = NULL): base_type(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override
    {
        QSize result = base_type::sizeHint(option, index);
        result.setWidth(result.width() + COLUMN_SPACING);
        return result;
    }
};

QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageConfigWidget)
{
    ui->setupUi(this);

    m_backupPool.model->setBackupRole(true);
    
    setupGrid(ui->mainStoragesTable, &m_mainPool);
    setupGrid(ui->backupStoragesTable, &m_backupPool);

    connect(ui->addExtStorageToMainBtn,     &QPushButton::clicked, this, [this]() {at_addExtStorage(true); });
    connect(ui->addExtStorageToBackupBtn,   &QPushButton::clicked, this, [this]() {at_addExtStorage(false); });

    connect(ui->rebuildMainButton,          &QPushButton::clicked, this, [this] { startRebuid(true); });
    connect(ui->rebuildBackupButton,        &QPushButton::clicked, this, [this] { startRebuid(false); });

    connect(ui->rebuildMainWidget,          &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(true); });
    connect(ui->rebuildBackupWidget,        &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(false); });

    connect(ui->pushButtonSchedule,         &QPushButton::clicked, this, &QnStorageConfigWidget::at_openBackupSchedule_clicked);
    connect(ui->pushButtonCameraSettings,   &QPushButton::clicked, this, &QnStorageConfigWidget::at_openBackupSettings_clicked);

    connect(ui->backupStartButton,          &QPushButton::clicked, this, &QnStorageConfigWidget::startBackup);
    connect(ui->backupStopButton,           &QPushButton::clicked, this, &QnStorageConfigWidget::cancelBackup);

    ui->comboBoxBackupType->addItem(tr("By schedule"), Qn::Backup_Schedule);
    ui->comboBoxBackupType->addItem(tr("Realtime mode"), Qn::Backup_RealTime);
    ui->comboBoxBackupType->addItem(tr("Never"), Qn::Backup_Disabled);
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain)
{
    QnStorageListModel* model = addToMain ? m_mainPool.model.data() : m_backupPool.model.data();

    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(model->modelData().storageProtocols);
    if(!dialog->exec())
        return;

    QnStorageSpaceData item = dialog->storage();
    if(!item.storageId.isNull())
        return;
    item.isUsedForWriting = true;
    item.isExternal = true;

    model->addModelData(item);
    updateColumnWidth();
}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, StoragePool* storagePool)
{

    tableView->setItemDelegate(new QnCustomItemDelegate(this));
    setWarningStyle(ui->storagesWarningLabel);
    ui->storagesWarningLabel->hide();

    
    tableView->setModel(storagePool->model.data());

    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->horizontalHeader()->setSectionResizeMode(QnStorageListModel::UrlColumn, QHeaderView::Stretch);

    connect(ui->comboBoxBackupType,   SIGNAL(currentIndexChanged(int)), this, SLOT(at_backupTypeComboBoxChange(int)));
    connect(tableView,         &QTableView::clicked,               this,   &QnStorageConfigWidget::at_eventsGrid_clicked);
    tableView->setMouseTracking(true);

    tableView->resizeColumnsToContents();
}

void QnStorageConfigWidget::at_backupTypeComboBoxChange(int index)
{
    ui->pushButtonSchedule->setEnabled(ui->comboBoxBackupType->itemData(index) == Qn::Backup_Schedule);
    ui->backupStartButton->setEnabled(ui->comboBoxBackupType->itemData(index) == Qn::Backup_Schedule); 

    m_serverUserAttrs.backupType = (Qn::BackupType) ui->comboBoxBackupType->itemData(index).toInt();
}

void QnStorageConfigWidget::updateFromSettings()
{
    sendStorageSpaceRequest();

    if (!m_server)
        return;

    {
        QnMediaServerUserAttributesPool::ScopedLock lk(QnMediaServerUserAttributesPool::instance(), m_server->getId());
        m_serverUserAttrs = *(*lk).data();
    }
    for (int i = 0; i < ui->comboBoxBackupType->count(); ++i) {
        if (ui->comboBoxBackupType->itemData(i) == m_serverUserAttrs.backupType) {
            ui->comboBoxBackupType->setCurrentIndex(i);
            break;
        }
    }
    m_cameraBackupSettings.clear();
}

void QnStorageConfigWidget::at_eventsGrid_clicked(const QModelIndex& index)
{
    bool isMain = index.model() == m_mainPool.model.data();

    if (index.column() == QnStorageListModel::ChangeGroupActionColumn)
    {
        if (isMain && m_mainPool.model->modelData().storages.size() <= 1)
            return;

        QnStorageListModel* fromModel = m_mainPool.model.data();
        QnStorageListModel* toModel = m_backupPool.model.data();
        if (!isMain)
            qSwap(fromModel, toModel);

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        fromModel->removeRow(index.row());
        toModel->addModelData(record);
        updateColumnWidth();
    }
    else if (index.column() == QnStorageListModel::RemoveActionColumn)
    {
        QnStorageListModel* model = isMain ? m_mainPool.model.data() : m_backupPool.model.data();

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        if (record.isExternal)
            model->removeRow(index.row());
        updateColumnWidth();
    }
}

void QnStorageConfigWidget::sendStorageSpaceRequest() {
    if (!m_server)
        return;
    m_mainPool.storageSpaceHandle = m_server->apiConnection()->getStorageSpaceAsync(true, this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
    m_backupPool.storageSpaceHandle = m_server->apiConnection()->getStorageSpaceAsync(false, this, SLOT(at_replyReceived(int, const QnStorageSpaceReply &, int)));
}

void QnStorageConfigWidget::at_replyReceived(int status, QnStorageSpaceReply reply, int handle) 
{
    if(status != 0) {
        //setBottomLabelText(tr("Could not load storages from server."));
        return;
    }
    
    qSort(reply.storages.begin(), reply.storages.end(), [](const QnStorageSpaceData &l, const QnStorageSpaceData &r) 
    {
        if (l.isExternal != r.isExternal)
            return l.isExternal < r.isExternal; 
        return l.url < r.url;
    });
    
    
    if (handle == m_mainPool.storageSpaceHandle)
        m_mainPool.model->setModelData(reply);
    else if (handle == m_backupPool.storageSpaceHandle)
        m_backupPool.model->setModelData(reply);
    updateColumnWidth();
}

void QnStorageConfigWidget::updateColumnWidth() {
    for (int i = 0; i < QnStorageListModel::ColumnCount; ++i) {

        /* Stretch url column */
        if (i == QnStorageListModel::UrlColumn)
            continue;

        int width = qMax(getColWidth(m_mainPool.model.data(), i), getColWidth(m_backupPool.model.data(), i));
        ui->mainStoragesTable->setColumnWidth(i, width + COLUMN_SPACING);
        ui->backupStoragesTable->setColumnWidth(i, width + COLUMN_SPACING);
    }
}

int QnStorageConfigWidget::getColWidth(const QAbstractItemModel* model, int col)
{
    int result = MIN_COL_WIDTH;
    QFont f = ui->mainStoragesTable->font();
    QFontMetrics fm(f);
    for (int i = 0; i < model->rowCount(); ++i)
    {
        QModelIndex index = model->index(i, col);
        int w = fm.width(model->data(index, Qn::TextWidthDataRole).toString());
        result = qMax(result, w);
    }

    return result;
}

void QnStorageConfigWidget::setServer(const QnMediaServerResourcePtr &server)
{
    if (m_server == server)
        return;

    if (m_server) {
        disconnect(m_server, nullptr, this, nullptr);
    }
    for (const auto &storage: m_storages)
        disconnect(storage, nullptr, this, nullptr);

    m_server = server;

    if (m_server) {
        connect(m_server,   &QnMediaServerResource::statusChanged,  this,   &QnStorageConfigWidget::updateRebuildInfo);
        connect(m_server,   &QnMediaServerResource::apiUrlChanged,  this,   &QnStorageConfigWidget::updateRebuildInfo);
        connect(m_server,   &QnMediaServerResource::statusChanged,  this,   &QnStorageConfigWidget::updateBackupInfo);
        connect(m_server,   &QnMediaServerResource::apiUrlChanged,  this,   &QnStorageConfigWidget::updateBackupInfo);
        m_storages = m_server->getStorages();
        for (const auto& storage: m_storages)
            connect(storage, &QnResource::statusChanged,            this,   &QnStorageConfigWidget::updateRebuildInfo);
        updateRebuildInfo();
        updateBackupInfo();
    }
}

void QnStorageConfigWidget::updateRebuildInfo() 
{
    if (isReadOnly())
        return;

    if (m_server->getStatus() == Qn::Online) {
        sendNextArchiveRequest(true);
        sendNextArchiveRequest(false);
    }
    else {
        updateRebuildUi(QnStorageScanData(), true);
        updateRebuildUi(QnStorageScanData(), false);
    }
}

void QnStorageConfigWidget::updateBackupInfo()
{
    if (isReadOnly())
        return;
    
    if (m_server->getStatus() == Qn::Online)
        sendNextBackupStatusRequest();
    else
        updateBackupUi(QnBackupStatusData());
}

void QnStorageConfigWidget::processStorages(QnStorageResourceList& result, const QList<QnStorageSpaceData>& modifiedData, bool isBackupPool) const
{
    for(const auto& storageData: modifiedData)
    {
        QnStorageResourcePtr storage = m_server->getStorageByUrl(storageData.url);
        if (storage) {
            if (storageData.isUsedForWriting != storage->isUsedForWriting() || isBackupPool != storage->isBackup()) 
            {
                storage->setUsedForWriting(storageData.isUsedForWriting);
                storage->setBackup(isBackupPool);
                result << storage;
            }
        }
        else {
            // create or remove new storage
            QnStorageResourcePtr storage(new QnClientStorageResource());
            if (!storageData.storageId.isNull())
                storage->setId(storageData.storageId);
            QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(lit("Storage"));
            if (resType)
                storage->setTypeId(resType->getId());
            storage->setName(QnUuid::createUuid().toString());
            storage->setParentId(m_server->getId());
            storage->setUrl(storageData.url);
            storage->setSpaceLimit(storageData.reservedSpace); //client does not change space limit anymore
            storage->setUsedForWriting(storageData.isUsedForWriting);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(isBackupPool);
            result << storage;
        }
    }
}

void QnStorageConfigWidget::submitToSettings()
{
    if (isReadOnly())
        return;

    QnStorageResourceList storagesToUpdate;
    ec2::ApiIdDataList storagesToRemove;

    processStorages(storagesToUpdate, m_mainPool.model->modelData().storages, false);
    processStorages(storagesToUpdate, m_backupPool.model->modelData().storages, true);

    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_mainPool.model->modelData().storages + m_backupPool.model->modelData().storages)
        newIdList << storageData.storageId;
    for (const auto& storage: m_server->getStorages()) {
        if (!newIdList.contains(storage->getId()))
            storagesToRemove.push_back(storage->getId());
    }

    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    if (!conn)
        return;

    //TODO: #GDM SafeMode
    if (!storagesToUpdate.isEmpty())
        conn->getMediaServerManager()->saveStorages(storagesToUpdate, this, []{});

    if (!storagesToRemove.empty())
        conn->getMediaServerManager()->removeStorages(storagesToRemove, this, []{});
    
    QnMediaServerUserAttributes serverUserAttrs;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(QnMediaServerUserAttributesPool::instance(), m_server->getId());
        serverUserAttrs = *(*lk).data();
    }
    if (serverUserAttrs != m_serverUserAttrs) 
    {
        qnResourcesChangesManager->saveServer(m_server, [this](const QnMediaServerResourcePtr &server) {
            server->setBackupType(m_serverUserAttrs.backupType);
            server->setBackupDOW(m_serverUserAttrs.backupDaysOfTheWeek);
            server->setBackupStart(m_serverUserAttrs.backupStart);
            server->setBackupDuration(m_serverUserAttrs.backupDuration);
            server->setBackupBitrate(m_serverUserAttrs.backupBitrate);
        });
    }

    QnVirtualCameraResourceList modifiedCameras;
    auto cameraBackupSettings = m_cameraBackupSettings;
    for (const auto& camSetting: cameraBackupSettings)
    {
        auto camRes = qnResPool->getResourceById<QnVirtualCameraResource>(camSetting.id);
        if (camRes && camSetting.backupType != camRes->getBackupType())
            modifiedCameras << camRes;
    }

    qnResourcesChangesManager->saveCameras(modifiedCameras, [cameraBackupSettings] (const QnVirtualCameraResourcePtr &camera) 
    {
        for (const auto& camSetting: cameraBackupSettings) 
        {
            if (camSetting.id == camera->getId()) {
                camera->setBackupType(camSetting.backupType);
                break;
            }
        }
    });

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
    
    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    if (isMain)
        ui->rebuildMainButton->setEnabled(false);
    else
        ui->rebuildBackupButton->setEnabled(false);

    storagePool.rebuildHandle = m_server->apiConnection()->doRebuildArchiveAsync (RebuildAction_Start, isMain, this, SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));
}

void QnStorageConfigWidget::cancelRebuild(bool isMain) {
    if (!m_server)
        return;
    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    storagePool.rebuildHandle = m_server->apiConnection()->doRebuildArchiveAsync(RebuildAction_Cancel, isMain, this, SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));
}

bool QnStorageConfigWidget::hasUnsavedChanges() const
{
    QnStorageResourceList storagesToUpdate;

    processStorages(storagesToUpdate, m_mainPool.model->modelData().storages, false);
    processStorages(storagesToUpdate, m_backupPool.model->modelData().storages, true);
    if (!storagesToUpdate.isEmpty())
        return true;

    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_mainPool.model->modelData().storages + m_backupPool.model->modelData().storages)
        newIdList << storageData.storageId;
    for (const auto& storage: m_server->getStorages()) {
        if (!newIdList.contains(storage->getId()))
            return true;
    }

    QnMediaServerUserAttributes serverUserAttrs;
    {
        QnMediaServerUserAttributesPool::ScopedLock lk(QnMediaServerUserAttributesPool::instance(), m_server->getId());
        serverUserAttrs = *(*lk).data();
    }
    if (serverUserAttrs != m_serverUserAttrs)
        return true;

    for (const auto& camSetting: m_cameraBackupSettings)
    {
        auto camRes = qnResPool->getResourceById<QnVirtualCameraResource>(camSetting.id);
        if (camRes && camSetting.backupType != camRes->getBackupType())
            return true;
    }

    return false;
}

void QnStorageConfigWidget::startBackup() {
    if (!m_server)
        return;

    if (hasUnsavedChanges())
    {
        int dialogResult = QMessageBox::question(
            this,
            tr("Confirm save changes"),
            tr("You have unsaved changes. This action requires to save it. Save changed and continue?"),
            QMessageBox::Yes | QMessageBox::Cancel);
        if(dialogResult != QMessageBox::Yes)
            return;
        submitToSettings();
    }

    ui->backupStartButton->setEnabled(false);
    m_server->apiConnection()->backupControlActionAsync (BackupAction_Start, this, SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
}

void QnStorageConfigWidget::cancelBackup() {
    if (!m_server)
        return;

    ui->backupStopButton->setEnabled(false);
    m_server->apiConnection()->backupControlActionAsync (BackupAction_Cancel, this, SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
}

void QnStorageConfigWidget::at_openBackupSchedule_clicked() 
{
    if (isReadOnly())
        return;

    auto scheduleDialog = new QnBackupScheduleDialog(this);
    scheduleDialog->updateFromSettings(m_serverUserAttrs);
    if (scheduleDialog->exec())
        scheduleDialog->submitToSettings(m_serverUserAttrs);
}

void QnStorageConfigWidget::at_openBackupSettings_clicked() 
{
    if (isReadOnly())
        return;

    // refresh
    BackupSettingsDataList cameraBackupSettings;
    for (const auto& camera: qnResPool->getAllCameras(QnResourcePtr(), true))
    {
        BackupSettingsData data;
        data.isChecked = true;
        data.id = camera->getId();
        data.backupType = camera->getBackupType();

        for (const auto& oldSetting: m_cameraBackupSettings) {
            if (oldSetting.id == data.id) {
                data = oldSetting;
                break;
            }
        }
        cameraBackupSettings.push_back(std::move(data));
    }
    m_cameraBackupSettings = cameraBackupSettings;


    auto backupSettingsDialog = new QnBackupSettingsDialog(this);
    backupSettingsDialog->updateFromSettings(m_cameraBackupSettings, m_serverUserAttrs);
    if (backupSettingsDialog->exec())
        backupSettingsDialog->submitToSettings(m_cameraBackupSettings, m_serverUserAttrs);
}

void QnStorageConfigWidget::at_backupStatusReply(int status, const QnBackupStatusData& reply, int handle)
{
    Q_UNUSED(handle)
    if (status == 0)
        updateBackupUi(reply);

    if (reply.state == Qn::BackupState_InProgress || status != 0)
        executeDelayed([this]{ sendNextBackupStatusRequest(); }, updateBackupStatusDelayMs);
}

void QnStorageConfigWidget::at_archiveRebuildReply(int status, const QnStorageScanData& reply, int handle)
{
    bool isMainPool = false;
    if (handle == m_mainPool.rebuildHandle)
        isMainPool = true;
    else if (handle == m_backupPool.rebuildHandle)
        isMainPool = false;
    else
        return; // unknown answer

    if (status == 0)
        updateRebuildUi(reply, isMainPool);

    if (reply.state > Qn::RebuildState_None || status != 0)
        executeDelayed([this, isMainPool]{ sendNextArchiveRequest(isMainPool); }, updateRebuildStatusDelayMs);
    else
        sendStorageSpaceRequest();
}

void QnStorageConfigWidget::sendNextArchiveRequest(bool isMain) {
    if (!m_server)
        return;

    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    storagePool.rebuildHandle = m_server->apiConnection()->doRebuildArchiveAsync (RebuildAction_ShowProgress, isMain, 
                                this, SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));
}

void QnStorageConfigWidget::sendNextBackupStatusRequest() 
{
    if (!m_server)
        return;
    
    m_server->apiConnection()->backupControlActionAsync(BackupAction_ShowProgress, this, SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
}

void QnStorageConfigWidget::updateBackupUi(const QnBackupStatusData& reply)
{
    QString status;

    if (reply.progress >= 0)
        ui->progressBarBackup->setValue(reply.progress * 100 + 0.5);

    bool showMessage = reply.state == Qn::BackupState_None && !ui->backupStartButton->isEnabled();
    ui->backupStartButton->setEnabled(reply.state == Qn::BackupState_None);
    ui->backupStopButton->setEnabled(reply.state == Qn::BackupState_InProgress);
    ui->stackedWidgetBackupInfo->setCurrentWidget(reply.state == Qn::BackupState_InProgress ? ui->backupProgressPage : ui->backupPreparePage);
    ui->stackedWidgetBackupInfo->setVisible(reply.state == Qn::BackupState_InProgress);

    if (showMessage)
    {
        ui->progressBarBackup->setValue(100); // 100%
        QMessageBox::information(this,
            tr("Finished"),
            tr("Archive backup is completed."));
    }
}

void QnStorageConfigWidget::updateRebuildUi(const QnStorageScanData& reply, bool isMainPool)
{
    StoragePool& pool = (isMainPool ? m_mainPool : m_backupPool);

    QnStorageScanData oldState = pool.prevRebuildState;
    pool.prevRebuildState = reply;

    if (isMainPool) {
        ui->rebuildMainWidget->loadData(reply);
        ui->rebuildMainButton->setEnabled(reply.state == Qn::RebuildState_None);
    }
    else {
        ui->rebuildBackupWidget->loadData(reply);
        ui->rebuildBackupButton->setEnabled(reply.state == Qn::RebuildState_None);
    }

    if (oldState.state == Qn::RebuildState_FullScan && reply.state == Qn::RebuildState_None) 
    {
        context()->instance<QnCameraDataManager>()->clearCache();
        QMessageBox::information(this,
            tr("Finished"),
            tr("Rebuilding archive index is completed."));
    }

    
}

