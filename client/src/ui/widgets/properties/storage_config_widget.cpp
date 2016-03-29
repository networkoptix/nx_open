#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"

#include <api/global_settings.h>
#include <api/model/storage_space_reply.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>

#include <boost/range/algorithm/count_if.hpp>

#include <camera/camera_data_manager.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>

#include <server/server_storage_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/dialogs/backup_schedule_dialog.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/models/storage_list_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <utils/common/qtimespan.h>

#include <common/common_globals.h>

namespace
{
    static const int kColumnSpacing = 8;
    static const int kMinColWidth = 60;

    class StoragesPoolFilterModel: public QSortFilterProxyModel {
    public:
        StoragesPoolFilterModel(bool isMainPool, QObject *parent = nullptr)
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

    class StorageTableItemDelegate: public QStyledItemDelegate
    {
        typedef QStyledItemDelegate base_type;

    public:
        explicit StorageTableItemDelegate(QObject *parent = NULL): base_type(parent) {}

        virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override
        {
            QSize result = base_type::sizeHint(option, index);
            result.setWidth(result.width() + kMinColWidth);
            return result;
        }
    };

    Qn::CameraBackupQualities extractQuality(QComboBox *comboBox)
    {
        return static_cast<Qn::CameraBackupQualities>(comboBox->currentData().toInt());
    }

    int findQualityIndex(QComboBox *comboBox
        , Qn::CameraBackupQualities quality)
    {
        return comboBox->findData(static_cast<int>(quality));
    }

    QnVirtualCameraResourceList getCurrentSelectedCameras()
    {
        const auto isSelectedForBackup = [](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->getActualBackupQualities() != Qn::CameraBackup_Disabled;
        };

        QnVirtualCameraResourceList serverCameras = qnResPool->getAllCameras(QnResourcePtr(), true);
        QnVirtualCameraResourceList selectedCameras = serverCameras.filtered(isSelectedForBackup);
        return selectedCameras;
    }

    const qint64 minDeltaForMessageMs = 1000ll * 3600 * 24;
    const qint64 updateStatusTimeoutMs = 5 * 1000;

} // anonymous namespace

 using boost::algorithm::any_of;

QnStorageConfigWidget::StoragePool::StoragePool()
    : rebuildCancelled(false)
{}


QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::StorageConfigWidget())
    , m_server()
    , m_model(new QnStorageListModel())
    , m_updateStatusTimer(new QTimer(this))
    , m_mainPool()
    , m_backupPool()
    , m_backupSchedule()
    , m_backupCancelled(false)
    , m_updating(false)
    , m_quality(qnGlobalSettings->backupQualities())
    , m_camerasToBackup()
{
    ui->setupUi(this);

    ui->comboBoxBackupType->addItem(tr("By Schedule"), Qn::Backup_Schedule);
    ui->comboBoxBackupType->addItem(tr("Real-Time"), Qn::Backup_RealTime);
    ui->comboBoxBackupType->addItem(tr("On Demand"),   Qn::Backup_Manual);

    setWarningStyle(ui->storagesWarningLabel);

    setWarningStyle(ui->backupStoragesAbsentLabel);
    ui->backupStoragesAbsentLabel->hide();

    setHelpTopic(this, Qn::ServerSettings_Storages_Help);
    setHelpTopic(ui->backupStoragesGroupBox, Qn::ServerSettings_StoragesBackup_Help);
    setHelpTopic(ui->backupControls, Qn::ServerSettings_StoragesBackup_Help);

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

    const auto chooseCamerasToBackup = [this]()
    {
        QScopedPointer<QnBackupCamerasDialog> dialog(new QnBackupCamerasDialog(mainWindow()));
        dialog->setSelectedResources(m_camerasToBackup);
        dialog->setWindowModality(Qt::ApplicationModal);
        if (dialog->exec() != QDialog::Accepted)
            return;

        updateCamerasForBackup(dialog->selectedResources().filtered<QnVirtualCameraResource>());
    };
    connect(ui->backupResourcesButton, &QPushButton::clicked, this, chooseCamerasToBackup);

    connect(ui->backupStartButton,          &QPushButton::clicked, this, &QnStorageConfigWidget::startBackup);
    connect(ui->backupStopButton,           &QPushButton::clicked, this, &QnStorageConfigWidget::cancelBackup);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildStatusChanged,    this, &QnStorageConfigWidget::at_serverRebuildStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupStatusChanged,     this, &QnStorageConfigWidget::at_serverBackupStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished,  this, &QnStorageConfigWidget::at_serverRebuildArchiveFinished);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupFinished,          this, &QnStorageConfigWidget::at_serverBackupFinished);

    connect(qnServerStorageManager, &QnServerStorageManager::storageAdded,                  this, [this](const QnStorageResourcePtr &storage) {
        if (m_server && storage->getParentServer() == m_server) {
            m_model->addStorage(QnStorageModelInfo(storage));
            emit hasChangesChanged();
        }
    });

    connect(qnServerStorageManager, &QnServerStorageManager::storageChanged,                this, [this](const QnStorageResourcePtr &storage) {
        m_model->updateStorage(QnStorageModelInfo(storage));
        emit hasChangesChanged();
    });

    connect(qnServerStorageManager, &QnServerStorageManager::storageRemoved,                this, [this](const QnStorageResourcePtr &storage) {
        m_model->removeStorage(QnStorageModelInfo(storage));
        emit hasChangesChanged();
    });


    connect(this, &QnAbstractPreferencesWidget::hasChangesChanged, this, [this]() {
        if (m_updating)
            return;

        updateBackupInfo();
        updateRebuildInfo();
        updateBackupWidgetsVisibility();
    });

    m_updateStatusTimer->setInterval(updateStatusTimeoutMs);
    connect(m_updateStatusTimer, &QTimer::timeout, this, [this] {
        if (isVisible())
            qnServerStorageManager->checkStoragesStatus(m_server);
    });

    at_backupTypeComboBoxChange(ui->comboBoxBackupType->currentIndex());

    QnResourceChangesListener *cameraBackupTypeListener = new QnResourceChangesListener(this);
    cameraBackupTypeListener->connectToResources<QnVirtualCameraResource>(&QnVirtualCameraResource::backupQualitiesChanged, this, &QnStorageConfigWidget::updateBackupInfo);

    initQualitiesCombo();
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{}

void QnStorageConfigWidget::restoreCamerasToBackup()
{
    updateCamerasForBackup(getCurrentSelectedCameras());
}

void QnStorageConfigWidget::resetQualities()
{
    m_quality = qnGlobalSettings->backupQualities();

    const auto qualityIndex = findQualityIndex(ui->qualityComboBox, m_quality);
    ui->qualityComboBox->setCurrentIndex(qualityIndex);
}

void QnStorageConfigWidget::initQualitiesCombo()
{
    static const auto qualitiesToString = [](Qn::CameraBackupQuality qualities) -> QString
    {
        switch (qualities)
        {
        case Qn::CameraBackup_LowQuality:
            return tr("Low-Res Streams", "Cameras Backup");
        case Qn::CameraBackup_HighQuality:
            return tr("Hi-Res Streams", "Cameras Backup");
        case Qn::CameraBackup_Both:
            return tr("All streams", "Cameras Backup");
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            break;
        }

        return QString();
    };

    QList<Qn::CameraBackupQuality> possibleQualities;
    possibleQualities
        << Qn::CameraBackup_HighQuality
        << Qn::CameraBackup_LowQuality
        << Qn::CameraBackup_Both;

    for (Qn::CameraBackupQuality value: possibleQualities)
        ui->qualityComboBox->addItem(qualitiesToString(value), static_cast<int>(value));

    resetQualities();

    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this, [this](int /* index */)
    {
        m_quality = extractQuality(ui->qualityComboBox);
        emit hasChangesChanged();
    });
}

void QnStorageConfigWidget::setReadOnlyInternal( bool readOnly ) {
    m_model->setReadOnly(readOnly);
    //TODO: #GDM #Safe Mode
}

void QnStorageConfigWidget::showEvent( QShowEvent *event ) {
    base_type::showEvent(event);
    m_updateStatusTimer->start();
    qnServerStorageManager->checkStoragesStatus(m_server);
}

void QnStorageConfigWidget::hideEvent( QHideEvent *event ) {
    base_type::hideEvent(event);

    resetQualities();
    restoreCamerasToBackup();

    m_updateStatusTimer->stop();
}

bool QnStorageConfigWidget::hasChanges() const {
    if (isReadOnly())
        return false;

    if (hasStoragesChanges(m_model->storages()))
        return true;

    if (m_quality != qnGlobalSettings->backupQualities())
        return true;

    // Check if cameras on !all! servers are different with selected
    if (getCurrentSelectedCameras().toSet() != m_camerasToBackup.toSet())
        return true;

    return (m_server->getBackupSchedule() != m_backupSchedule);
}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain) {
    if (!m_server || isReadOnly())
        return;

    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(qnServerStorageManager->protocols(m_server));
    dialog->setCurrentServerStorages(m_model->storages());
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
    updateBackupWidgetsVisibility();
    ui->backupStoragesAbsentLabel->hide();

    emit hasChangesChanged();
}

void QnStorageConfigWidget::setupGrid(QTableView* tableView, bool isMainPool)
{
    tableView->setItemDelegate(new StorageTableItemDelegate(this));

    StoragesPoolFilterModel* filterModel = new StoragesPoolFilterModel(isMainPool, this);
    filterModel->setSourceModel(m_model.data());
    tableView->setModel(filterModel);

    tableView->resizeColumnsToContents();
    tableView->horizontalHeader()->setSectionsClickable(false);
    tableView->horizontalHeader()->setStretchLastSection(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    tableView->horizontalHeader()->setSectionResizeMode(QnStorageListModel::UrlColumn, QHeaderView::Stretch);

    connect(tableView,         &QTableView::clicked,               this,   &QnStorageConfigWidget::at_eventsGrid_clicked);

    tableView->setMouseTracking(true);
}

void QnStorageConfigWidget::at_backupTypeComboBoxChange(int index)
{
    const auto currentBackupType = static_cast<Qn::BackupType>(ui->comboBoxBackupType->itemData(index).toInt());

    m_backupSchedule.backupType = currentBackupType;
    ui->pushButtonSchedule->setEnabled(currentBackupType == Qn::Backup_Schedule);
    emit hasChangesChanged();
}

void QnStorageConfigWidget::loadDataToUi() {
    if (!m_server)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    loadStoragesFromResources();
    m_backupSchedule = m_server->getBackupSchedule();

    const auto backupTypeIndex = ui->comboBoxBackupType->findData(m_backupSchedule.backupType);
    ui->comboBoxBackupType->setCurrentIndex(backupTypeIndex);

    updateRebuildInfo();
    updateBackupInfo();
    updateBackupWidgetsVisibility();

    ui->storagesWarningStackedWidget->setCurrentWidget(ui->pageSpacer);
    ui->backupStoragesAbsentLabel->hide();
}

void QnStorageConfigWidget::loadStoragesFromResources() {
    NX_ASSERT(m_server, Q_FUNC_INFO, "Server must exist here");

    QnStorageModelInfoList storages;
    for (const QnStorageResourcePtr &storage: m_server->getStorages())
        storages.append(QnStorageModelInfo(storage));
    m_model->setStorages(storages);

    updateColumnWidth();
    updateBackupWidgetsVisibility();
}

void QnStorageConfigWidget::at_eventsGrid_clicked(const QModelIndex& index)
{
    if (!m_server || isReadOnly())
        return;

    auto showWarningLabelIfNeeded = [this]
    {
        bool backupStoragesExist = any_of(m_model->storages(), [] (const QnStorageModelInfo &info)
        {
            return info.isBackup;
        });

        // Show "Backup won't be performed." when:
        // 1. Not read only
        // 2. No backup storages.
        // 3. Backup is configured (not 'On Demand')
        const bool showStoragesAbsent = (!isReadOnly() &&
            !backupStoragesExist &&
            m_backupSchedule.backupType != Qn::Backup_Manual);
        ui->backupStoragesAbsentLabel->setVisible(showStoragesAbsent);
    };

    QnStorageModelInfo record = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

    if (index.column() == QnStorageListModel::ChangeGroupActionColumn)
    {
        if (!m_model->canMoveStorage(record))
            return;

        record.isBackup = !record.isBackup;
        m_model->updateStorage(record);
        updateColumnWidth();
        updateBackupWidgetsVisibility();

        if (record.isBackup)
            ui->backupStoragesAbsentLabel->hide();
        else
            showWarningLabelIfNeeded();
    }
    else if (index.column() == QnStorageListModel::RemoveActionColumn)
    {
        if (m_model->canRemoveStorage(record))
            m_model->removeStorage(record);

        updateColumnWidth();
        updateBackupWidgetsVisibility();

        /* Check if we have removed the last backup storage. */
        if (record.isBackup)
            showWarningLabelIfNeeded();
    }
    else if (index.column() == QnStorageListModel::CheckBoxColumn) {
        if (index.data(Qt::CheckStateRole) == Qt::Unchecked && hasChanges())
            ui->storagesWarningStackedWidget->setCurrentWidget(ui->pageStoragesWarning);
        else if (!hasChanges())
            ui->storagesWarningStackedWidget->setCurrentWidget(ui->pageSpacer);
    }

    emit hasChangesChanged();
}

void QnStorageConfigWidget::updateColumnWidth()
{
   /*
    auto scrollbarOffset = [](QnTableView* table)
    {
        return table && table->verticalScrollBar() && table->verticalScrollBar()->isVisible()
            ? table->verticalScrollBar()->width()
            : 0;
    };

    int mainScrollbarOffset = scrollbarOffset(ui->mainStoragesTable);
    int backupScrollbarOffset = scrollbarOffset(ui->backupStoragesTable);
    */

    for (int i = 0; i < QnStorageListModel::ColumnCount; ++i)
    {
        /* Stretch url column */
        if (i == QnStorageListModel::UrlColumn)
            continue;

        /* By default columns in both tables must have the same width. */
        int mainWidth = getColWidth(i) + kColumnSpacing;
        int backupWidth = mainWidth;

        /* //TODO: #GDM make this code work, for now scrollbars can move columns
        if (i == QnStorageListModel::ChangeGroupActionColumn)
        {
            mainWidth += backupScrollbarOffset;
            backupWidth += mainScrollbarOffset;
        }
        */

        ui->mainStoragesTable->setColumnWidth(i, mainWidth);
        ui->backupStoragesTable->setColumnWidth(i, backupWidth);
    }
}

int QnStorageConfigWidget::getColWidth(int col)
{
    int result = kMinColWidth;
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

    if (m_server)
        disconnect(m_server, &QnMediaServerResource::backupScheduleChanged, this, nullptr);

    m_server = server;
    m_model->setServer(server);
    restoreCamerasToBackup();

    if (m_server)
        connect(m_server, &QnMediaServerResource::backupScheduleChanged, this, [this]() {
            /* Current changes may be lost, it's OK. */
            m_backupSchedule = m_server->getBackupSchedule();
            emit hasChangesChanged();
        });
}

void QnStorageConfigWidget::updateRebuildInfo() {
    for (int i = 0; i < static_cast<int>(QnServerStoragesPool::Count); ++i) {
        QnServerStoragesPool pool = static_cast<QnServerStoragesPool>(i);
        updateRebuildUi(pool, qnServerStorageManager->rebuildStatus(m_server, pool));
    }
}

void QnStorageConfigWidget::updateBackupInfo() {
    updateBackupUi(qnServerStorageManager->backupStatus(m_server)
        , m_camerasToBackup.size());
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
            NX_ASSERT(storage->getId() == storageData.id, Q_FUNC_INFO, "Id's must be equal");

            storage->setUsedForWriting(storageData.isUsed);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(storageData.isBackup);
            storage->setSpaceLimit(storageData.reservedSpace);

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

    applyCamerasToBackup(m_camerasToBackup, m_quality);
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

    /* Make sure scheduled backup will stop in no backup storages left after 'Apply' button. */
    bool backupStoragesExist = any_of(m_model->storages(), [](const QnStorageModelInfo &info)
    {
        return info.isBackup;
    });
    if (!backupStoragesExist)
    {
        m_backupSchedule.backupType = Qn::Backup_Manual;
        const auto backupTypeIndex = ui->comboBoxBackupType->findData(m_backupSchedule.backupType);
        ui->comboBoxBackupType->setCurrentIndex(backupTypeIndex);
    }

    if (m_backupSchedule != m_server->getBackupSchedule())
    {
        qnResourcesChangesManager->saveServer(m_server, [this](const QnMediaServerResourcePtr &server) {
            server->setBackupSchedule(m_backupSchedule);
        });
    }

    ui->storagesWarningStackedWidget->setCurrentWidget(ui->pageSpacer);
    ui->backupStoragesAbsentLabel->hide();
    emit hasChangesChanged();
}

void QnStorageConfigWidget::startRebuid(bool isMain) {
    if (!m_server)
        return;

    int warnResult = QnMessageBox::warning(
        this,
        tr("Warning!"),
        tr("You are about to launch the archive re-synchronization routine.") + L'\n'
        + tr("ATTENTION! Your hard disk usage will be increased during re-synchronization process! Depending on the total size of archive it can take several hours.") + L'\n'
        + tr("This process is only necessary if your archive folders have been moved, renamed or replaced. You can cancel rebuild operation at any moment without data loss.") + L'\n'
        + tr("Are you sure you want to continue?"),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
    );
    if (warnResult != QDialogButtonBox::Ok)
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
    if (!scheduleDialog->exec() || isReadOnly())
        return;

    scheduleDialog->submitToSettings(m_backupSchedule);
    emit hasChangesChanged();
}

bool QnStorageConfigWidget::canStartBackup(const QnBackupStatusData& data
    , int selectedCamerasCount
    , QString *info)
{
    auto error = [info](const QString &error) -> bool
    {
        if (info)
            *info = error;
        return false;
    };

    if (data.state != Qn::BackupState_None)
        return error(tr("Backup is already in progress."));

    const auto isCorrectStorage = [](const QnStorageModelInfo &storage)
    {
        return storage.isWritable && storage.isUsed && storage.isBackup;
    };

    if (!any_of(m_model->storages(), isCorrectStorage))
        return error(tr("Select at least one backup storage."));

    if ((m_backupSchedule.backupType == Qn::Backup_Schedule)
        && !m_backupSchedule.isValid())
    {
        return error(tr("Backup Schedule is invalid."));
    }

    if (hasChanges())
        return error(tr("Apply changes before starting backup."));

    if (m_backupSchedule.backupType == Qn::Backup_RealTime)
        return false;

    if (!selectedCamerasCount)
    {
        const auto text = QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Select at least one device to start backup.")
            , tr("Select at least one camera to start backup."));
        return error(text);
    }

    const auto rebuildStatusState = [this](QnServerStoragesPool type)
    {
        return qnServerStorageManager->rebuildStatus(m_server, type).state;
    };

    if ((rebuildStatusState(QnServerStoragesPool::Main) != Qn::RebuildState_None)
        || (rebuildStatusState(QnServerStoragesPool::Backup) != Qn::RebuildState_None))
    {
        return error(tr("Cannot start backup while archive index rebuild is in progress."));
    }

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

void QnStorageConfigWidget::updateBackupWidgetsVisibility() {
    bool backupStoragesExist = any_of(m_model->storages(), [](const QnStorageModelInfo &info) {
        return info.isBackup;
    });

    /* Notify about backup possibility if there are less than two valid storages in the system. */

    const auto writableDevices = boost::count_if(m_model->storages(), [this](const QnStorageModelInfo &info)
    {
        return info.isWritable;
    });


    ui->backupStoragesGroupBox->setVisible(backupStoragesExist);
    ui->backupControls->setVisible(backupStoragesExist);

    // Show warning when:
    // 1. Not read only
    // 2. No storages where isBackup == true
    // 3. Writable storages count is less than 2
    const bool lessThanTwoStorages = (writableDevices < 2);
    const bool showWarning = (!isReadOnly() &&
        !backupStoragesExist && lessThanTwoStorages);
    ui->backupOptionLabel->setVisible(showWarning);
}

void QnStorageConfigWidget::updateBackupUi(const QnBackupStatusData& reply
    , int overallSelectedCameras)
{
    QString status;

    bool backupInProgress = reply.state == Qn::BackupState_InProgress;
    if (backupInProgress)
        ui->progressBarBackup->setValue(reply.progress * 100 + 0.5);
    else
        ui->progressBarBackup->setValue(0);

    QString backupInfo;
    bool canStartBackup = this->canStartBackup(
        reply, overallSelectedCameras, &backupInfo);
    ui->backupWarningLabel->setText(backupInfo);

    bool realtime = m_backupSchedule.backupType == Qn::Backup_RealTime;

    //TODO: #GDM discuss texts
    QString backedUpTo = realtime
        ? tr("In Real-Time mode all data is backed up continuously.") + L' ' + setWarningStyleHtml(tr("Notice: Only further recording will be backed up. Backup process will ignore existing footage."))
        : reply.backupTimeMs > 0
        ? tr("Archive backup is completed up to: %1.").arg(backupPositionToString(reply.backupTimeMs))
        : tr("Backup was never started.");

    ui->backupTimeLabel->setText(backedUpTo);

    ui->backupStartButton->setEnabled(canStartBackup);
    ui->backupStopButton->setEnabled(backupInProgress);
    ui->stackedWidgetBackupInfo->setCurrentWidget(backupInProgress ? ui->backupProgressPage : ui->backupPreparePage);
    ui->comboBoxBackupType->setEnabled(!backupInProgress);

    updateSelectedCamerasCaption(overallSelectedCameras);
}

void QnStorageConfigWidget::updateSelectedCamerasCaption(int selectedCamerasCount)
{
    if (!m_server)
        return;

    const auto getNumberedCaption = [this, selectedCamerasCount]() -> QString
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
              tr("%n Camera(s)", "", selectedCamerasCount)
            , tr("%n Device(s)", "", selectedCamerasCount));
    };

    const auto getSimpleCaption = []()
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("No devices selected"), tr("No cameras selected"));
    };

    const bool numberedCaption = (selectedCamerasCount > 0);
    const QString caption = (numberedCaption
        ? getNumberedCaption() : getSimpleCaption());

    auto newPalette = palette();
    if (!numberedCaption)
        setWarningStyle(&newPalette);

    const auto icon = qnResIconCache->icon(
        numberedCaption ? QnResourceIconCache::Camera : QnResourceIconCache::Offline
        , numberedCaption ? false : true);

    ui->backupResourcesButton->setIcon(icon);
    ui->backupResourcesButton->setPalette(newPalette);
    ui->backupResourcesButton->setText(caption);
}

void QnStorageConfigWidget::updateCamerasForBackup(const QnVirtualCameraResourceList &cameras)
{
    if (m_camerasToBackup.toSet() == cameras.toSet())
        return;

    m_camerasToBackup = cameras;

    updateBackupInfo();
    emit hasChangesChanged();
}

void QnStorageConfigWidget::applyCamerasToBackup(const QnVirtualCameraResourceList &cameras
    , Qn::CameraBackupQualities quality)
{
    qnGlobalSettings->setBackupQualities(quality);

    const auto qualityForCamera = [cameras, quality](const QnVirtualCameraResourcePtr &camera)
    {
        return (cameras.contains(camera) ? quality : Qn::CameraBackup_Disabled);
    };

    /* Update all default cameras and all cameras that we have changed. */
    const auto modifiedFilter = [qualityForCamera](const QnVirtualCameraResourcePtr &camera)
    {
        return camera->getBackupQualities() != qualityForCamera(camera);
    };

    const auto modified = qnResPool->getAllCameras(QnResourcePtr(), true).filtered(modifiedFilter);

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified
        , [qualityForCamera](const QnVirtualCameraResourcePtr &camera)
    {
        camera->setBackupQualities(qualityForCamera(camera));
    });
}

void QnStorageConfigWidget::updateRebuildUi(QnServerStoragesPool pool, const QnStorageScanData& reply)
{
    m_model->updateRebuildInfo(pool, reply);

    bool isMainPool = pool == QnServerStoragesPool::Main;

    /* Here we must check actual backup schedule, not ui-selected. */
    bool backupIsInProgress = m_server && m_server->getBackupSchedule().backupType != Qn::Backup_RealTime &&
        qnServerStorageManager->backupStatus(m_server).state != Qn::BackupState_None;

    ui->addExtStorageToMainBtn->setEnabled(!backupIsInProgress);
    ui->addExtStorageToBackupBtn->setEnabled(!backupIsInProgress);

    bool canStartRebuild =
            m_server
        &&  reply.state == Qn::RebuildState_None
        &&  !hasChanges()
        &&  any_of(m_model->storages(), [this, isMainPool](const QnStorageModelInfo &info) {
                return info.isWritable
                    && info.isBackup != isMainPool
                    && info.isOnline
                    && m_model->storageIsActive(info);   /* Ignoring newly added external storages until Apply pressed. */
            })
        && !backupIsInProgress
    ;

    if (isMainPool) {
        ui->rebuildMainWidget->loadData(reply);
        ui->rebuildMainButton->setEnabled(canStartRebuild);
        ui->rebuildMainButton->setVisible(reply.state == Qn::RebuildState_None);
    }
    else {
        ui->rebuildBackupWidget->loadData(reply);
        ui->rebuildBackupButton->setEnabled(canStartRebuild);
        ui->rebuildBackupButton->setVisible(reply.state == Qn::RebuildState_None);
    }
}

void QnStorageConfigWidget::at_serverRebuildStatusChanged( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool, const QnStorageScanData &status ) {
    if (server != m_server)
        return;

    updateRebuildUi(pool, status);
    updateBackupInfo();
}

void QnStorageConfigWidget::at_serverBackupStatusChanged( const QnMediaServerResourcePtr &server, const QnBackupStatusData &status ) {
    if (server != m_server)
        return;

    updateBackupUi(status, m_camerasToBackup.size());
    updateRebuildInfo();
}

void QnStorageConfigWidget::at_serverRebuildArchiveFinished( const QnMediaServerResourcePtr &server, QnServerStoragesPool pool ) {
    if (server != m_server)
        return;

    if (!isVisible())
        return;

    bool isMain = (pool == QnServerStoragesPool::Main);
    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    if (!storagePool.rebuildCancelled)
        QnMessageBox::information(this,
            tr("Finished"),
            tr("Rebuilding archive index is completed."));
    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::at_serverBackupFinished( const QnMediaServerResourcePtr &server ) {
    if (server != m_server)
        return;

    if (!isVisible())
        return;

    if (!m_backupCancelled)
        QnMessageBox::information(this,
            tr("Finished"),
            tr("Backup is finished"));
    m_backupCancelled = false;
}
