#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"
#include <api/model/storage_space_reply.h>
#include <core/resource/client_storage_resource.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/style/warning_style.h>
#include "utils/common/event_processors.h"
#include <core/resource/media_server_resource.h>
#include "ui/models/storage_list_model.h"
#include "api/app_server_connection.h"
#include "api/media_server_connection.h"
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <camera/camera_data_manager.h>
#include "ui/dialogs/backup_schedule_dialog.h"
#include "ui/dialogs/backup_settings_dialog.h"
#include "core/resource/media_server_user_attributes.h"
#include "core/resource_management/resources_changes_manager.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_resource.h"
#include "ui/common/ui_resource_name.h"

namespace {
    //setting free space to zero, since now client does not change this value, so it must keep current value
    const qint64 defaultReservedSpace = 0;  //5ll * 1024ll * 1024ll * 1024ll;

    static const int rebuildProgressPageIndex = 1;
    static const int rebuildPreparePageIndex = 0;
    static const int COLUMN_SPACING = 8;

} // anonymous namespace


QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageConfigWidget)
{
    ui->setupUi(this);
    
    setupGrid(ui->mainStoragesTable, &m_mainPool);
    setupGrid(ui->backupStoragesTable, &m_backupPool);

    connect(ui->addExtStorageToMainBtn, &QPushButton::clicked, this, [this]() {at_addExtStorage(true); });
    connect(ui->addExtStorageToBackupBtn, &QPushButton::clicked, this, [this]() {at_addExtStorage(false); });

    connect(ui->rebuildMainBtn, &QPushButton::clicked, this, &QnStorageConfigWidget::at_rebuildButton_clicked);
    connect(ui->rebuildBackupBtn, &QPushButton::clicked, this, &QnStorageConfigWidget::at_rebuildButton_clicked);

    connect(ui->rebuildStopButtonMain, &QPushButton::clicked, this, &QnStorageConfigWidget::at_rebuildCancel_clicked);
    connect(ui->rebuildStopButtonBackup, &QPushButton::clicked, this, &QnStorageConfigWidget::at_rebuildCancel_clicked);

    connect(ui->pushButtonSchedule, &QPushButton::clicked, this, &QnStorageConfigWidget::at_openBackupSchedule_clicked);
    connect(ui->pushButtonCameraSettings, &QPushButton::clicked, this, &QnStorageConfigWidget::at_openBackupSettings_clicked);

    connect(ui->backupStartButton, &QPushButton::clicked, this, &QnStorageConfigWidget::at_backupButton_clicked);
    connect(ui->backupStopButton, &QPushButton::clicked, this, &QnStorageConfigWidget::at_backupCancel_clicked);

    m_backupPool.model->setBackupRole(true);

    ui->comboBoxBackupType->addItem(tr("By schedule"), Qn::Backup_Schedule);
    ui->comboBoxBackupType->addItem(tr("Realtime mode"), Qn::Backup_RealTime);
    ui->comboBoxBackupType->addItem(tr("Never"), Qn::Backup_Disabled);

    m_scheduleDialog = new QnBackupScheduleDialog(this);
    m_camerabackupSettingsDialog = new QnBackupSettingsDialog(this);
}

QnStorageConfigWidget::~QnStorageConfigWidget()
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

void QnStorageConfigWidget::at_addExtStorage(bool addToMain)
{
    QnStorageListModel* model = addToMain ? m_mainPool.model : m_backupPool.model;

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
}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, StoragePool* storagePool)
{
    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->setItemDelegate(new QnCustomItemDelegate(this));
    setWarningStyle(ui->storagesWarningLabel);
    ui->storagesWarningLabel->hide();

    //QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    //signalizer->setEventType(QEvent::ContextMenu);
    //tableView->installEventFilter(signalizer);

    storagePool->model = new QnStorageListModel();
    tableView->setModel(storagePool->model);

    connect(ui->comboBoxBackupType,   SIGNAL(currentIndexChanged(int)), this, SLOT(at_backupTypeComboBoxChange(int)));
    connect(tableView,         &QTableView::clicked,               this,   &QnStorageConfigWidget::at_eventsGrid_clicked);
    tableView->setMouseTracking(true);
}

void QnStorageConfigWidget::at_backupTypeComboBoxChange(int index)
{
    ui->pushButtonSchedule->setEnabled(ui->comboBoxBackupType->itemData(index) == Qn::Backup_Schedule);
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
    bool isMain = index.model() == m_mainPool.model;

    if (index.column() == QnStorageListModel::ChangeGroupActionColumn)
    {
        QnStorageListModel* fromModel = m_mainPool.model;
        QnStorageListModel* toModel = m_backupPool.model;
        if (!isMain)
            qSwap(fromModel, toModel);

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        fromModel->removeRow(index.row());
        toModel->addModelData(record);
    }
    else if (index.column() == QnStorageListModel::RemoveActionColumn)
    {
        QnStorageListModel* model = isMain ? m_mainPool.model : m_backupPool.model;

        QVariant data = index.data(Qn::StorageSpaceDataRole);
        if (!data.canConvert<QnStorageSpaceData>())
            return;
        QnStorageSpaceData record = data.value<QnStorageSpaceData>();
        if (record.isExternal)
            model->removeRow(index.row());
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

void QnStorageConfigWidget::processStorages(QnStorageResourceList& result, const QList<QnStorageSpaceData>& modifiedData, bool isBackupPool)
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

void QnStorageConfigWidget::at_rebuildButton_clicked() 
{
    if (!m_server)
        return;

    QPushButton* button = dynamic_cast<QPushButton*>(sender());
    if (!button)
        return;

    bool isMain = (button == ui->rebuildMainBtn);

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
    button->setDisabled(true);
    storagePool.rebuildHandle = m_server->apiConnection()->doRebuildArchiveAsync (RebuildAction_Start, isMain, this, SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));
}

void QnStorageConfigWidget::at_backupButton_clicked()
{
    if (!m_server)
        return;

    QPushButton* button = dynamic_cast<QPushButton*>(sender());
    if (!button)
        return;

    button->setDisabled(true);
    m_server->apiConnection()->backupControlActionAsync (BackupAction_Start, this, SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
}

void QnStorageConfigWidget::at_rebuildCancel_clicked() 
{
    if (!m_server)
        return;

    QPushButton* button = dynamic_cast<QPushButton*>(sender());
    if (!button)
        return;

    bool isMain = (button == ui->rebuildStopButtonMain);

    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    button->setDisabled(true);
    storagePool.rebuildHandle = m_server->apiConnection()->doRebuildArchiveAsync (RebuildAction_Cancel, isMain, this, SLOT(at_archiveRebuildReply(int, const QnStorageScanData &, int)));

}

void QnStorageConfigWidget::at_backupCancel_clicked() 
{
    if (!m_server)
        return;

    QPushButton* button = dynamic_cast<QPushButton*>(sender());
    if (!button)
        return;

    button->setDisabled(true);
    m_server->apiConnection()->backupControlActionAsync (BackupAction_Cancel, this, SLOT(at_backupStatusReply(int, const QnBackupStatusData &, int)));
}

void QnStorageConfigWidget::at_openBackupSchedule_clicked() 
{
    if (!m_server)
        return;

    m_scheduleDialog->updateFromSettings(m_serverUserAttrs);
    if (m_scheduleDialog->exec())
        m_scheduleDialog->submitToSettings(m_serverUserAttrs);
}

void QnStorageConfigWidget::at_openBackupSettings_clicked() 
{
    if (!m_server)
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


    m_camerabackupSettingsDialog->updateFromSettings(m_cameraBackupSettings, m_serverUserAttrs);
    if (m_camerabackupSettingsDialog->exec())
        m_camerabackupSettingsDialog->submitToSettings(m_cameraBackupSettings, m_serverUserAttrs);
}

void QnStorageConfigWidget::at_backupStatusReply(int status, const QnBackupStatusData& reply, int handle)
{
    Q_UNUSED(status)
    Q_UNUSED(handle)

    updateBackupUi(reply);

    if (reply.state == Qn::BackupState_InProgress)
        QTimer::singleShot(500, this, SLOT(sendNextBackupStatusRequest()));
}

void QnStorageConfigWidget::at_archiveRebuildReply(int status, const QnStorageScanData& reply, int handle)
{
    Q_UNUSED(status);
    bool isMainPool = false;
    if (handle == m_mainPool.rebuildHandle)
        isMainPool = true;
    else if (handle == m_backupPool.rebuildHandle)
        isMainPool = false;
    else
        return; // unknown answer

    updateRebuildUi(reply, isMainPool);

    if (reply.state > Qn::RebuildState_None) {
        if (isMainPool)
            QTimer::singleShot(500, this, SLOT(sendNextArchiveRequestForMain()));
        else
            QTimer::singleShot(500, this, SLOT(sendNextArchiveRequestForBackup()));
    }
    else
        sendStorageSpaceRequest();
}

void QnStorageConfigWidget::sendNextArchiveRequestForMain()
{
    sendNextArchiveRequest(true);
}

void QnStorageConfigWidget::sendNextArchiveRequestForBackup()
{
    sendNextArchiveRequest(false);
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

    if (reply.state == Qn::BackupState_None && !ui->backupStartButton->isEnabled())
    {
        ui->progressBarBackup->setValue(100); // 100%
        QMessageBox::information(this,
            tr("Finished"),
            tr("Archive backup is completed."));
    }

    ui->backupStartButton->setEnabled(reply.state == Qn::BackupState_None);
    ui->backupStopButton->setEnabled(reply.state == Qn::BackupState_InProgress);

    ui->stackedWidgetBackupInfo->setCurrentIndex(reply.state == Qn::BackupState_InProgress ? rebuildProgressPageIndex : rebuildPreparePageIndex);
    ui->stackedWidgetBackupInfo->setVisible(reply.state == Qn::BackupState_InProgress);
}

void QnStorageConfigWidget::updateRebuildUi(const QnStorageScanData& reply, bool isMainPool)
{
    StoragePool& pool = (isMainPool ? m_mainPool : m_backupPool);

    QnStorageScanData oldState = pool.prevRebuildState;
    pool.prevRebuildState = reply;

    //ui->rebuildGroupBox->setEnabled(reply.state != Qn::RebuildState_Unknown);

    QPushButton* rebuildStartButton;
    QPushButton* rebuildStopButton;
    QProgressBar* rebuildProgressBar;
    QnWordWrappedLabel* rebuildStatusLabel;
    QStackedWidget* stackedWidget;

    if (isMainPool) {
        rebuildStartButton = ui->rebuildMainBtn;
        rebuildStopButton = ui->rebuildStopButtonMain;
        rebuildProgressBar = ui->rebuildProgressBarMain;
        rebuildStatusLabel = ui->rebuildStatusLabelMain;
        stackedWidget = ui->stackedWidgetMain;
    }
    else {
        rebuildStartButton = ui->rebuildBackupBtn;
        rebuildStopButton = ui->rebuildStopButtonBackup;
        rebuildProgressBar = ui->rebuildProgressBarBackup;
        rebuildStatusLabel = ui->rebuildStatusLabelBackup;
        stackedWidget = ui->stackedWidgetBackup;
    }

    QString status;
    if (!reply.path.isEmpty()) {
        if (reply.state == Qn::RebuildState_FullScan)
            status = tr("Rebuild archive index for storage '%1' is in progress").arg(reply.path);
        else if (reply.state == Qn::RebuildState_PartialScan)
            status = tr("Fast archive scan for storage '%1' is in progress").arg(reply.path);
    }
    else if (reply.state == Qn::RebuildState_FullScan)
        status = tr("Rebuild archive index for storage '%1' is in progress").arg(reply.path);
    else if (reply.state == Qn::RebuildState_PartialScan)
        status = tr("Fast archive scan for storage '%1' is in progress").arg(reply.path);

    rebuildStatusLabel->setText(status);

    if (reply.progress >= 0)
        rebuildProgressBar->setValue(reply.progress * 100 + 0.5);
    rebuildStartButton->setEnabled(reply.state == Qn::RebuildState_None);
    rebuildStopButton->setEnabled(reply.state == Qn::RebuildState_FullScan);

    if (oldState.state == Qn::RebuildState_FullScan && reply.state == Qn::RebuildState_None) 
    {
        context()->instance<QnCameraDataManager>()->clearCache();
        QMessageBox::information(this,
            tr("Finished"),
            tr("Rebuilding archive index is completed."));
    }

    stackedWidget->setCurrentIndex(reply.state > Qn::RebuildState_None ? rebuildProgressPageIndex : rebuildPreparePageIndex);
    stackedWidget->setVisible(reply.state > Qn::RebuildState_None);
}

