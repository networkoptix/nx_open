#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/count_if.hpp>

#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

#include <api/global_settings.h>
#include <api/model/storage_space_reply.h>
#include <api/model/backup_status_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <camera/camera_data_manager.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
#include <server/server_storage_manager.h>
#include <translation/datetime_formatter.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/dialogs/backup_settings_dialog.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/models/storage_list_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/widgets/storage_space_slider.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/analytics/utils.h>

using namespace nx;
using namespace nx::vms::client::desktop;

using ServerTimeWatcher = nx::vms::client::core::ServerTimeWatcher;

namespace
{
    static const int kMinimumColumnWidth = 80;

    class StoragesSortModel : public QSortFilterProxyModel
    {
    public:
        StoragesSortModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    protected:
        virtual bool lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const override
        {
            auto left = leftIndex.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();
            auto right = rightIndex.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

            /* Local storages should go first. */
            if (left.isExternal != right.isExternal)
                return right.isExternal;

            /* Group storages by plugin. */
            if (left.storageType != right.storageType)
                return QString::compare(left.storageType, right.storageType, Qt::CaseInsensitive) < 0;

            return QString::compare(left.url, right.url, Qt::CaseInsensitive) < 0;
        }
    };

    class StorageTableItemDelegate : public QStyledItemDelegate
    {
        typedef QStyledItemDelegate base_type;

    public:
        explicit StorageTableItemDelegate(ItemViewHoverTracker* hoverTracker, QObject* parent = nullptr) :
            base_type(parent),
            m_hoverTracker(hoverTracker),
            m_editedRow(-1)
        {
        }

        virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QSize result = base_type::sizeHint(option, index);

            if (index.column() == QnStorageListModel::StoragePoolColumn
                && index.flags().testFlag(Qt::ItemIsEditable))
            {
                result.rwidth() += style::Metrics::kArrowSize + style::Metrics::kStandardPadding;
            }

            if (index.column() != QnStorageListModel::CheckBoxColumn)
                result.setWidth(qMax(result.width(), kMinimumColumnWidth));

            return result;
        }

        virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);

            bool editableColumn = opt.state.testFlag(QStyle::State_Enabled)
              && index.column() == QnStorageListModel::StoragePoolColumn
              && index.flags().testFlag(Qt::ItemIsEditable);

            bool hovered = m_hoverTracker && m_hoverTracker->hoveredIndex() == index;
            bool beingEdited = m_editedRow == index.row();

            bool hoveredRow = m_hoverTracker && m_hoverTracker->hoveredIndex().row() == index.row();
            bool hasActiveAction = index.column() == QnStorageListModel::ActionsColumn
                && index.data(Qn::ItemMouseCursorRole).toInt() == Qt::PointingHandCursor;
                // TODO: add a separate data role for such cases?

            // Hide actions when their row is not hovered.
            if (hasActiveAction && !hoveredRow)
                return;

            auto storage = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

            /* Set disabled style for unchecked rows: */
            if (!index.sibling(index.row(), QnStorageListModel::CheckBoxColumn).data(Qt::CheckStateRole).toBool())
                opt.state &= ~QStyle::State_Enabled;

            // Set proper color for action text buttons.
            if (index.column() == QnStorageListModel::ActionsColumn)
            {
                if (hasActiveAction && hovered)
                    opt.palette.setColor(QPalette::Text, colorTheme()->color("light14"));
                else if (hasActiveAction)
                    opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::WindowText));
                else // Either hidden (has no text) or selected, we can use 'Selected' style for both.
                    opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Light));
            }


            /* Set warning color for inaccessible storages: */
            if (index.column() == QnStorageListModel::StoragePoolColumn && !storage.isOnline)
                opt.palette.setColor(QPalette::Text, qnGlobals->errorTextColor());

            /* Set proper color for hovered storage type column: */
            if (!opt.state.testFlag(QStyle::State_Enabled))
                opt.palette.setCurrentColorGroup(QPalette::Disabled);
            if (editableColumn && hovered)
                opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::ButtonText));

            /* Draw item: */
            QStyle* style = option.widget ? option.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

            /* Draw arrow if editable storage type column: */
            if (editableColumn)
            {
                QStyleOption arrowOption = opt;
                arrowOption.rect.setLeft(arrowOption.rect.left() +
                    style::Metrics::kStandardPadding +
                    opt.fontMetrics.width(opt.text) +
                    style::Metrics::kArrowSize / 2);

                arrowOption.rect.setWidth(style::Metrics::kArrowSize);

                auto arrow = beingEdited
                    ? QStyle::PE_IndicatorArrowUp
                    : QStyle::PE_IndicatorArrowDown;

                QStyle* style = option.widget ? option.widget->style() : QApplication::style();
                style->drawPrimitive(arrow, &arrowOption, painter);
            }
        }

        virtual QWidget* createEditor(QWidget* /*parent*/, const QStyleOptionViewItem& /*option*/,
            const QModelIndex& /*index*/) const override
        {
            return nullptr;
        }

        void setEditedRow(int row)
        {
            m_editedRow = row;
        }

    private:
        QPointer<ItemViewHoverTracker> m_hoverTracker;
        int m_editedRow;
    };

    class ColumnResizer: public QObject
    {
    public:
        using QObject::QObject;

    protected:
        bool eventFilter(QObject* object, QEvent* event)
        {
            auto view = qobject_cast<nx::vms::client::desktop::TreeView *>(object);

            if (view && event->type() == QEvent::Resize)
            {
                int occupiedWidth = 0;
                for (int i = 1; i < view->model()->columnCount(); ++i)
                    occupiedWidth += view->sizeHintForColumn(i);

                int urlWidth = view->sizeHintForColumn(QnStorageListModel::UrlColumn);
                view->setColumnWidth(QnStorageListModel::UrlColumn,
                    qMin(urlWidth, view->width() - occupiedWidth));
            }

            return false;
        }
    };

    QnVirtualCameraResourceList getCurrentSelectedCameras(QnResourcePool* resourcePool)
    {
        const auto isSelectedForBackup = [](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->getActualBackupQualities() != nx::vms::api::CameraBackupQuality::CameraBackup_Disabled;
        };

        QnVirtualCameraResourceList serverCameras = resourcePool->getAllCameras(QnResourcePtr(), true);
        QnVirtualCameraResourceList selectedCameras = serverCameras.filtered(isSelectedForBackup);
        return selectedCameras;
    }

    const qint64 kMinDeltaForMessageMs = 1000ll * 3600 * 24;
    const qint64 kUpdateStatusTimeoutMs = 5 * 1000;

} // anonymous namespace

using boost::algorithm::any_of;

QnStorageConfigWidget::StoragePool::StoragePool() :
    rebuildCancelled(false)
{
}

QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageConfigWidget()),
    m_server(),
    m_model(new QnStorageListModel()),
    m_columnResizer(new ColumnResizer(this)),
    m_updateStatusTimer(new QTimer(this)),
    m_updateLabelsTimer(new QTimer(this)),
    m_storagePoolMenu(new QMenu(this)),
    m_mainPool(),
    m_backupPool(),
    m_backupSchedule(),
    m_lastPerformedBackupTimeMs(0),
    m_nextScheduledBackupTimeMs(0),
    m_backupCancelled(false),
    m_updating(false),
    m_quality(qnGlobalSettings->backupQualities()),
    m_camerasToBackup(),
    m_backupNewCameras(qnGlobalSettings->backupNewCamerasByDefault()),
    m_realtimeBackupMovie(new QMovie(devicePixelRatio() == 1
        ? lit(":/skin/archive_backup/backup_in_progress.gif")
        : lit(":/skin/archive_backup/backup_in_progress@2x.gif")))
{
    ui->setupUi(this);

    ui->backupTimeLabel->setForegroundRole(QPalette::Light);
    ui->backupScheduleLabel->setForegroundRole(QPalette::Light);
    ui->realtimeBackupStatusLabel->setForegroundRole(QPalette::Light);

    ui->cannotStartBackupLabel->setVisible(false);
    ui->backupStartButton->setVisible(false);

    ui->realtimeBackupControlButton->hide(); /* Unused in the current version. */
    ui->estimatedTimeLabel->hide(); /* Unused in the current version. */

    ui->backupSettingsButtonDuplicate->setText(ui->backupSettingsButton->text());
    connect(ui->backupSettingsButtonDuplicate, &QPushButton::clicked, ui->backupSettingsButton, &QPushButton::clicked);

    ui->progressBarBackup->setFormat(lit("%1\t%p%").arg(tr("Backup is in progress...")));

    m_storagePoolMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_storagePoolMenu->addAction(tr("Main"))->setData(false);
    m_storagePoolMenu->addAction(tr("Backup"))->setData(true);

    setHelpTopic(this, Qn::ServerSettings_Storages_Help);
    setHelpTopic(ui->backupGroupBox, Qn::SystemSettings_Server_Backup_Help);

    ui->rebuildBackupButtonHint->addHintLine(tr("Reindexing can fix problems with archive or "
        "backup if they have been lost or damaged, or if some hardware has been replaced."));
    setHelpTopic(ui->rebuildBackupButtonHint, Qn::ServerSettings_ArchiveRestoring_Help);

    auto hoverTracker = new ItemViewHoverTracker(ui->storageView);
    hoverTracker->setMouseCursorRole(Qn::ItemMouseCursorRole);

    auto itemDelegate = new StorageTableItemDelegate(hoverTracker, this);
    ui->storageView->setItemDelegate(itemDelegate);

    auto switchItemDelegate = new SwitchItemDelegate(this);
    switchItemDelegate->setHideDisabledItems(true);
    ui->storageView->setItemDelegateForColumn(
        QnStorageListModel::CheckBoxColumn, switchItemDelegate);

    StoragesSortModel* sortModel = new StoragesSortModel(this);
    sortModel->setSourceModel(m_model.data());
    ui->storageView->setModel(sortModel);
    ui->storageView->sortByColumn(0, Qt::AscendingOrder);
    ui->storageView->header()->setStretchLastSection(false);
    ui->storageView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->storageView->header()->setSectionResizeMode(QnStorageListModel::UrlColumn, QHeaderView::Fixed);
    ui->storageView->header()->setSectionResizeMode(QnStorageListModel::SeparatorColumn, QHeaderView::Stretch);
    ui->storageView->setMouseTracking(true);
    ui->storageView->installEventFilter(m_columnResizer.data());

    auto itemClicked = [this, itemDelegate](const QModelIndex& index)
    {
        itemDelegate->setEditedRow(index.row());
        ui->storageView->update(index);
        at_storageView_clicked(index);
        itemDelegate->setEditedRow(-1);
        ui->storageView->update(index);
    };

    connect(ui->storageView, &TreeView::clicked, this, itemClicked);

    connect(ui->backupPages, &QStackedWidget::currentChanged,
        this, &QnStorageConfigWidget::updateRealtimeBackupMovieStatus);

    connect(ui->addExtStorageToMainBtn,     &QPushButton::clicked, this, [this]() { at_addExtStorage(true); });

    connect(ui->rebuildMainButton,          &QPushButton::clicked, this, [this] { startRebuid(true); });
    connect(ui->rebuildBackupButton,        &QPushButton::clicked, this, [this] { startRebuid(false); });
    connect(ui->rebuildMainWidget,          &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(true); });
    connect(ui->rebuildBackupWidget,        &QnStorageRebuildWidget::cancelRequested, this, [this]{ cancelRebuild(false); });

    connect(ui->backupSettingsButton,       &QPushButton::clicked, this, &QnStorageConfigWidget::invokeBackupSettings);

    connect(ui->backupStartButton,     &QPushButton::clicked, this, &QnStorageConfigWidget::startBackup);
    connect(ui->backupStopButton,      &QPushButton::clicked, this, &QnStorageConfigWidget::cancelBackup);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildStatusChanged,
        this, &QnStorageConfigWidget::at_serverRebuildStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupStatusChanged,
        this, &QnStorageConfigWidget::at_serverBackupStatusChanged);
    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished,
        this, &QnStorageConfigWidget::at_serverRebuildArchiveFinished);
    connect(qnServerStorageManager, &QnServerStorageManager::serverBackupFinished,
        this, &QnStorageConfigWidget::at_serverBackupFinished);

    connect(qnServerStorageManager, &QnServerStorageManager::storageAdded, this,
        [this](const QnStorageResourcePtr& storage)
        {
            if (m_server && storage->getParentServer() == m_server)
            {
                m_model->addStorage(QnStorageModelInfo(storage));
                emit hasChangesChanged();
            }
        });

    connect(qnServerStorageManager, &QnServerStorageManager::storageChanged, this,
        [this](const QnStorageResourcePtr& storage)
        {
            m_model->updateStorage(QnStorageModelInfo(storage));
            emit hasChangesChanged();
        });

    connect(qnServerStorageManager, &QnServerStorageManager::storageRemoved, this,
        [this](const QnStorageResourcePtr& storage)
        {
            m_model->removeStorage(QnStorageModelInfo(storage));
            emit hasChangesChanged();
        });

    connect(this, &QnAbstractPreferencesWidget::hasChangesChanged, this,
        [this]()
        {
            if (m_updating)
                return;

            updateBackupInfo();
            updateRebuildInfo();
        });

    m_updateLabelsTimer->setInterval(1000);
    connect(m_updateLabelsTimer, &QTimer::timeout, this, &QnStorageConfigWidget::updateIntervalLabels);

    m_updateStatusTimer->setInterval(kUpdateStatusTimeoutMs);
    connect(m_updateStatusTimer, &QTimer::timeout, this,
        [this]
        {
            if (isVisible())
                qnServerStorageManager->checkStoragesStatus(m_server);
        });

    QnResourceChangesListener* cameraBackupTypeListener = new QnResourceChangesListener(this);
    cameraBackupTypeListener->connectToResources<QnVirtualCameraResource>(
        &QnVirtualCameraResource::backupQualitiesChanged, this, &QnStorageConfigWidget::updateBackupInfo);

    /* By [Left] disable storage, by [Right] enable storage: */
    installEventHandler(ui->storageView, QEvent::KeyPress, this,
        [this, itemClicked](QObject* object, QEvent* event)
        {
            Q_UNUSED(object);
            auto index = ui->storageView->currentIndex();
            if (!index.isValid())
                return;

            int key = static_cast<QKeyEvent*>(event)->key();
            switch (key)
            {
                case Qt::Key_Left:
                    index = index.sibling(index.row(), QnStorageListModel::CheckBoxColumn);
                    if (index.data(Qt::CheckStateRole).toInt() == Qt::Unchecked)
                        break;
                    ui->storageView->model()->setData(index, Qt::Unchecked, Qt::CheckStateRole);
                    itemClicked(index);
                    break;

                case Qt::Key_Right:
                    index = index.sibling(index.row(), QnStorageListModel::CheckBoxColumn);
                    if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
                        break;
                    ui->storageView->model()->setData(index, Qt::Checked, Qt::CheckStateRole);
                    itemClicked(index);
                    break;

                case Qt::Key_Space:
                    index = index.sibling(index.row(), QnStorageListModel::StoragePoolColumn);
                    itemClicked(index);
                    break;

                default:
                    return;
            }
        });
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{
}

void QnStorageConfigWidget::restoreCamerasToBackup()
{
    updateCamerasForBackup(getCurrentSelectedCameras(resourcePool()));
}

void QnStorageConfigWidget::setReadOnlyInternal(bool readOnly)
{
    m_model->setReadOnly(readOnly);
    // TODO: #GDM #Safe Mode
}

void QnStorageConfigWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_updateStatusTimer->start();
    m_updateLabelsTimer->start();
    qnServerStorageManager->checkStoragesStatus(m_server);
}

void QnStorageConfigWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);

    m_quality = qnGlobalSettings->backupQualities();
    m_backupNewCameras = qnGlobalSettings->backupNewCamerasByDefault();
    restoreCamerasToBackup();

    m_updateStatusTimer->stop();
    m_updateLabelsTimer->stop();
}

bool QnStorageConfigWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    if (hasStoragesChanges(m_model->storages()))
        return true;

    if (m_quality != qnGlobalSettings->backupQualities())
        return true;

    if (m_backupNewCameras != qnGlobalSettings->backupNewCamerasByDefault())
        return true;

    // Check if cameras on !all! servers are different with selected
    if (getCurrentSelectedCameras(resourcePool()).toSet() != m_camerasToBackup.toSet())
        return true;

    return (m_server->getBackupSchedule() != m_backupSchedule);
}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain)
{
    if (!m_server || isReadOnly())
        return;

    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, this));
    dialog->setProtocols(qnServerStorageManager->protocols(m_server));
    dialog->setCurrentServerStorages(m_model->storages());
    if (!dialog->exec())
        return;

    QnStorageModelInfo item = dialog->storage();
    /* Check if somebody have added this storage right now */
    if (item.id.isNull())
    {
        // New storages has tested and ready to add
        item.id = QnStorageResource::fillID(m_server->getId(), item.url);
        item.isBackup = !addToMain;
        item.isUsed = true;
        item.isOnline = true;
    }

    m_model->addStorage(item);  // Adds or updates storage model data
    emit hasChangesChanged();
}

void QnStorageConfigWidget::loadDataToUi()
{
    if (!m_server)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    loadStoragesFromResources();
    m_backupSchedule = m_server->getBackupSchedule();
    m_camerasToBackup = getCurrentSelectedCameras(resourcePool());

    updateWarnings();

    updateRebuildInfo();
    updateBackupInfo();
}

void QnStorageConfigWidget::loadStoragesFromResources()
{
    NX_ASSERT(m_server, "Server must exist here");

    QnStorageModelInfoList storages;
    for (const QnStorageResourcePtr& storage: m_server->getStorages())
        storages.append(QnStorageModelInfo(storage));

    m_model->setStorages(storages);
}

void QnStorageConfigWidget::at_storageView_clicked(const QModelIndex& index)
{
    if (!m_server || isReadOnly())
        return;

    QnStorageModelInfo record = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

    if (index.column() == QnStorageListModel::StoragePoolColumn)
    {
        if (!m_model->canChangeStoragePool(record) || !index.flags().testFlag(Qt::ItemIsEditable))
            return;

        auto findAction = [this](bool isBackup) -> QAction*
        {
            for (auto action : m_storagePoolMenu->actions())
                if (action->data().toBool() == isBackup)
                    return action;

            return nullptr;
        };

        m_storagePoolMenu->setActiveAction(findAction(record.isBackup));

        QPoint point = ui->storageView->viewport()->mapToGlobal(
            ui->storageView->visualRect(index).bottomLeft()) + QPoint(0, 1);

        auto selection = m_storagePoolMenu->exec(point);
        if (!selection)
            return;

        bool isBackup = selection->data().toBool();
        if (record.isBackup == isBackup)
            return;

        record.isBackup = isBackup;
        m_model->updateStorage(record);
    }
    else if (index.column() == QnStorageListModel::ActionsColumn)
    {
        if (m_model->canRemoveStorage(record))
        {
            // Network storage.
            m_model->removeStorage(record);
        }
        else if (m_model->canStoreAnalytics(record))
        {
            // Local storage, may be used to store analytics.
            if (record.id != m_model->metadataStorageId())
            {
                const auto storageId = record.id;
                confirmNewMetadataStorage(storageId);
            }
        }
    }
    else if (index.column() == QnStorageListModel::CheckBoxColumn)
    {
        updateWarnings();
    }

    emit hasChangesChanged();
}

void QnStorageConfigWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    if (m_server)
        m_server->disconnect(this);

    m_server = server;
    m_model->setServer(server);
    restoreCamerasToBackup();

    if (m_server)
    {
        connect(m_server, &QnMediaServerResource::backupScheduleChanged, this,
            [this]()
            {
                /* Current changes may be lost, it's OK. */
                m_backupSchedule = m_server->getBackupSchedule();
                emit hasChangesChanged();
            });

        connect(m_server, &QnMediaServerResource::propertyChanged, this,
            [this](const QnResourcePtr&, const QString& key)
            {
                if (key == QnMediaServerResource::kMetadataStorageIdKey)
                {
                    m_model->setMetadataStorageId(m_server->metadataStorageId());
                // Now metadata storage is changed instantly, so we don't fire hasChangedChanged().
                }
        });
    }
}

void QnStorageConfigWidget::updateRebuildInfo()
{
    for (int i = 0; i < static_cast<int>(QnServerStoragesPool::Count); ++i)
    {
        QnServerStoragesPool pool = static_cast<QnServerStoragesPool>(i);
        updateRebuildUi(pool, qnServerStorageManager->rebuildStatus(m_server, pool));
    }
}

void QnStorageConfigWidget::updateBackupInfo()
{
    updateBackupUi(qnServerStorageManager->backupStatus(m_server), m_camerasToBackup.size());
}

void QnStorageConfigWidget::applyStoragesChanges(QnStorageResourceList& result, const QnStorageModelInfoList& storages) const
{
    for (const auto& storageData: storages)
    {
        QnStorageResourcePtr storage = m_server->getStorageByUrl(storageData.url);
        if (storage)
        {
            if (storageData.isUsed != storage->isUsedForWriting() || storageData.isBackup != storage->isBackup())
            {
                storage->setUsedForWriting(storageData.isUsed);
                storage->setBackup(storageData.isBackup);
                result << storage;
            }
        }
        else
        {
            QnClientStorageResourcePtr storage = QnClientStorageResource::newStorage(m_server, storageData.url);
            NX_ASSERT(storage->getId() == storageData.id, "Id's must be equal");

            storage->setUsedForWriting(storageData.isUsed);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(storageData.isBackup);
            storage->setSpaceLimit(storageData.reservedSpace);

            resourcePool()->addResource(storage);
            result << storage;
        }
    }
}

bool QnStorageConfigWidget::hasStoragesChanges(const QnStorageModelInfoList& storages) const
{
    if (!m_server)
        return false;

    for (const auto& storageData: storages)
    {
        /* New storage was added. */
        QnStorageResourcePtr storage = resourcePool()->getResourceById<QnStorageResource>(storageData.id);
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
    nx::vms::api::IdDataList storagesToRemove;

    applyCamerasToBackup(m_camerasToBackup, m_quality);
    applyStoragesChanges(storagesToUpdate, m_model->storages());

    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_model->storages())
        newIdList << storageData.id;

    for (const auto& storage: m_server->getStorages())
    {
        if (!newIdList.contains(storage->getId()))
        {
            storagesToRemove.push_back(storage->getId());
            resourcePool()->removeResource(storage);
        }
    }

    if (!storagesToUpdate.empty())
        qnServerStorageManager->saveStorages(storagesToUpdate);

    if (!storagesToRemove.empty())
        qnServerStorageManager->deleteStorages(storagesToRemove);

    if (m_backupSchedule != m_server->getBackupSchedule())
    {
        qnResourcesChangesManager->saveServer(m_server,
            [this](const QnMediaServerResourcePtr& server)
            {
                server->setBackupSchedule(m_backupSchedule);
            });
    }

    updateWarnings();
    emit hasChangesChanged();
}

void QnStorageConfigWidget::startRebuid(bool isMain)
{
    if (!m_server)
        return;

    const auto extras =
        tr("Depending on the total size of the archive, reindexing can take up to several hours.")
        + L'\n' +tr("Reindexing is only necessary if your archive folders have been moved, renamed or deleted.")
        + L'\n' +tr("You can cancel this operation at any moment without data loss.")
        + L'\n' +tr("Continue anyway?");

    const auto warnResult = QnMessageBox::warning(this,
        tr("Hard disk load will increase significantly"), extras,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Ok);

    if (warnResult != QDialogButtonBox::Ok)
        return;

    if (!qnServerStorageManager->rebuildServerStorages(m_server, isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
        return;

    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::cancelRebuild(bool isMain)
{
    if (!qnServerStorageManager->cancelServerStoragesRebuild(m_server, isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
        return;

    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);
    storagePool.rebuildCancelled = true;
}

void QnStorageConfigWidget::startBackup()
{
    if (!m_server)
        return;

    if (qnServerStorageManager->backupServerStorages(m_server))
    {
        ui->backupStartButton->setEnabled(false);
        m_backupCancelled = false;
    }
}

void QnStorageConfigWidget::cancelBackup()
{
    if (!m_server)
        return;

    if (qnServerStorageManager->cancelBackupServerStorages(m_server))
    {
        ui->backupStopButton->setEnabled(false);
        m_backupCancelled = true;
    }
}

bool QnStorageConfigWidget::canStartBackup(const QnBackupStatusData& data,
    int selectedCamerasCount, QString* info)
{
    auto error =
        [info](const QString& error) -> bool
        {
            if (info)
                *info = error;
            return false;
        };

    if (data.state != Qn::BackupState_None)
        return error(tr("Backup is already in progress."));

    QnStorageModelInfoList validStorages;
    for (const auto& storage: m_model->storages())
    {
        if (!storage.isWritable)
            continue;
        validStorages << storage;
    }

    // TODO: #GDM what if there is only one storage - and it is backup?
    // TODO: #GDM what if there are no storages at all?

    if (validStorages.size() < 2)
        return error(tr("Add more drives to use them as backup storage."));

    const auto isEnabledBackupStorage =
        [](const QnStorageModelInfo& storage)
        {
            return storage.isUsed && storage.isBackup;
        };

    // TODO: #GDM what if storage is not used, so we should just enable it?
    if (!any_of(validStorages, isEnabledBackupStorage))
        return error(tr("Change \"Main\" to \"Backup\" for some of the storage above to enable backup."));

    if (hasChanges())
        return error(tr("Apply changes to start backup."));

    if (selectedCamerasCount == 0)
    {
        const auto text = QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Select at least one device in the Backup Settings to start backup."),
            tr("Select at least one camera in the Backup Settings to start backup."));
        return error(text);
    }

    if (m_backupSchedule.backupType == vms::api::BackupType::realtime)
        return true;

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

quint64 QnStorageConfigWidget::nextScheduledBackupTimeMs() const
{
    if (m_backupSchedule.backupType != vms::api::BackupType::scheduled || !m_backupSchedule.isValid())
        return 0;

    // Backup schedule struct always contains server time.
    const QDateTime current = ServerTimeWatcher::serverTime(m_server, qnSyncTime->currentMSecsSinceEpoch());
    const qint64 currentTimeMs = current.toMSecsSinceEpoch();

    static const int kDaysPerWeek = 7;
    int currentDayOfWeek = current.date().dayOfWeek() - 1; // zero-based

    QDateTime scheduled = current;
    scheduled.setTime(QTime(0, 0).addSecs(m_backupSchedule.backupStartSec));

    for (int i = 0; i <= kDaysPerWeek; ++i, scheduled = scheduled.addDays(1))
    {
        const auto dayIndex = (currentDayOfWeek + i) % kDaysPerWeek; // Zero-based.
        const auto dayOfWeekFlag = vms::api::dayOfWeek(Qt::DayOfWeek(dayIndex + 1));

        if ((m_backupSchedule.backupDaysOfTheWeek & dayOfWeekFlag) == 0)
            continue;

        qint64 scheduledTimeMs = scheduled.toMSecsSinceEpoch();
        if (scheduledTimeMs > currentTimeMs)
            return scheduledTimeMs;
    }

    NX_ASSERT(false, "Should never get here");
    return 0;
}

QString QnStorageConfigWidget::backupPositionToString(qint64 backupTimeMs)
{
    QDateTime backupDateTime = ServerTimeWatcher::serverTime(m_server, backupTimeMs);
    if (context()->instance<ServerTimeWatcher>()->timeMode() == ServerTimeWatcher::clientTimeMode)
        backupDateTime = backupDateTime.toLocalTime();
    return datetime::toString(backupDateTime);
}

QString QnStorageConfigWidget::intervalToString(qint64 backupTimeMs)
{
    auto deltaMs = qnSyncTime->currentDateTime().toMSecsSinceEpoch() - backupTimeMs;
    const auto inTheFuture = deltaMs < 0;
    if (inTheFuture)
        deltaMs = -deltaMs;

    if (inTheFuture || deltaMs > kMinDeltaForMessageMs)
    {
        const auto duration = std::chrono::milliseconds(std::abs(deltaMs));
        using HumanReadable = nx::vms::client::core::HumanReadable;
        const auto timeSpan = HumanReadable::timeSpan(duration);
        return inTheFuture
            ? tr("in %1").arg(timeSpan)
            : tr("%1 before now").arg(timeSpan);
    }

    return QString();
}

void QnStorageConfigWidget::updateRealtimeBackupMovieStatus(int index)
{
    const auto page = ui->backupPages->widget(index);
    if (page == ui->backupRealtimePage)
        m_realtimeBackupMovie->start();
    else
        m_realtimeBackupMovie->stop();
}

void QnStorageConfigWidget::updateBackupUi(const QnBackupStatusData& reply, int overallSelectedCameras)
{
    m_lastPerformedBackupTimeMs = m_nextScheduledBackupTimeMs = 0;

    bool backupInProgress = reply.state == Qn::BackupState_InProgress;
    ui->backupStopButton->setEnabled(backupInProgress);

    if (backupInProgress)
    {
        ui->progressBarBackup->setValue(reply.progress * 100 + 0.5);
        ui->backupPages->setCurrentWidget(ui->backupProgressPage);
    }
    else
    {
        QString backupInfo;
        bool canStartBackup = this->canStartBackup(
            reply, overallSelectedCameras, &backupInfo);

        if (m_backupSchedule.backupType == vms::api::BackupType::realtime)
        {
            ui->realtimeBackupWarningLabel->setText(backupInfo);
            ui->realtimeBackupWarningLabel->setVisible(!canStartBackup);

            if (canStartBackup)
            {
                ui->realtimeBackupStatusLabel->setText(tr("Realtime backup is active..."));
                QnHiDpiWorkarounds::setMovieToLabel(ui->realtimeIconLabel, m_realtimeBackupMovie.data());
                updateRealtimeBackupMovieStatus(ui->backupPages->currentIndex());
            }
            else
            {
                ui->realtimeBackupStatusLabel->setText(tr("Realtime backup is set up."));
                ui->realtimeIconLabel->setPixmap(qnSkin->pixmap("archive_backup/backup_ready.png"));
            }

            ui->backupPages->setCurrentWidget(ui->backupRealtimePage);
        }
        else
        {
            m_lastPerformedBackupTimeMs = reply.backupTimeMs;

            bool backupSchedule = m_backupSchedule.backupType == vms::api::BackupType::scheduled;
            ui->backupScheduleWidget->setVisible(backupSchedule);
            if (backupSchedule)
                m_nextScheduledBackupTimeMs = nextScheduledBackupTimeMs();

            updateIntervalLabels();

            ui->cannotStartBackupLabel->setText(backupInfo);
            ui->cannotStartBackupLabel->setVisible(!canStartBackup);
            ui->backupStartButton->setVisible(canStartBackup);
            ui->backupStartButton->setEnabled(true);

            ui->backupPages->setCurrentWidget(ui->backupInformationPage);
        }
    }
}

void QnStorageConfigWidget::confirmNewMetadataStorage(const QnUuid& storageId)
{
    const auto updateServerSettings =
        [this](
            const nx::vms::api::MetadataStorageChangePolicy policy,
            const QnUuid& storageId)
        {
            qnGlobalSettings->setMetadataStorageChangePolicy(policy);
            qnGlobalSettings->synchronizeNow();

            qnResourcesChangesManager->saveServer(m_server,
                [storageId](const QnMediaServerResourcePtr& server)
                {
                    server->setMetadataStorageId(storageId);
                });
        };

    if (nx::analytics::hasActiveObjectEngines(commonModule(), m_server->getId()))
    {
        // Metadata storage has been changed, we need to do something with database.

        QnMessageBox msgBox(
            QnMessageBoxIcon::Question,
            tr("What to do with current analytics data?"),
            tr("Current analytics data will not be automatically moved to another location"
                " and will become inaccessible. You can keep it and manually move later,"
                " or delete permanently."
                "\n"
                "If you intended to move analytics data to another storage location,"
                " please contact support before proceeding."),
            QDialogButtonBox::StandardButtons(),
            QDialogButtonBox::NoButton,
            this);

        const auto deleteButton = msgBox.addButton(tr("Delete"),
            QDialogButtonBox::ButtonRole::AcceptRole, Qn::ButtonAccent::Warning);
        const auto keepButton = msgBox.addButton(tr("Keep"),
            QDialogButtonBox::ButtonRole::AcceptRole, Qn::ButtonAccent::NoAccent);

        // This dialog uses non-standard layout, so we can't use ButtonRole::CancelRole.
        const auto cancelButton = msgBox.addButton(tr("Cancel"),
            QDialogButtonBox::ButtonRole::AcceptRole, Qn::ButtonAccent::NoAccent);

        cancelButton->setFocus();

        // Since we don't have a button with CancelRole, we need to set it manually.
        msgBox.setEscapeButton(cancelButton);

        msgBox.exec();

        if (msgBox.clickedButton() == deleteButton)
        {
            updateServerSettings(
                nx::vms::api::MetadataStorageChangePolicy::remove,
                storageId);
            updateWarnings();
        }

        if (msgBox.clickedButton() == keepButton)
        {
            updateServerSettings(
                nx::vms::api::MetadataStorageChangePolicy::keep,
                storageId);
            updateWarnings();
        }

        // If the cancel button has been clicked, do nothing...
    }
    else
    {
        // We don't have analytics database yet, so just assign a new storage.

        updateServerSettings(
            nx::vms::api::MetadataStorageChangePolicy::keep, //< Just to be sure...
            storageId);
        updateWarnings();
    }

}

void QnStorageConfigWidget::updateWarnings()
{
    bool analyticsIsOnSystemDrive = false;
    bool analyticsIsOnDisabledStorage = false;
    bool hasDisabledStorage = false;
    bool hasUsbStorage = false;

    for (const auto& storageData: m_model->storages())
    {
        const auto storage = resourcePool()->getResourceById<QnStorageResource>(storageData.id);
        if (!storage || storage->getParentId() != m_server->getId())
            continue;

        if (storageData.id == m_model->metadataStorageId())
        {
            if (storage->statusFlag() & Qn::StorageStatus::system)
                analyticsIsOnSystemDrive = true;

            if (!storageData.isUsed)
                analyticsIsOnDisabledStorage = true;
        }

        // Storage was not modified.
        if (storageData.isUsed == storage->isUsedForWriting())
            continue;

        if (!storageData.isUsed)
            hasDisabledStorage = true;

        //TODO: use PartitionType enum value here instead of the serialized literal
        if (storageData.isUsed && storageData.storageType == lit("usb"))
            hasUsbStorage = true;
    }

    ui->analyticsOnSystemDriveWarningLabel->setVisible(analyticsIsOnSystemDrive);
    ui->analyticsOnDisabledStorageWarningLabel->setVisible(analyticsIsOnDisabledStorage);
    ui->storagesWarningLabel->setVisible(hasDisabledStorage);
    ui->usbWarningLabel->setVisible(hasUsbStorage);
}

void QnStorageConfigWidget::updateIntervalLabels()
{
    if (m_lastPerformedBackupTimeMs <= 0)
    {
        ui->backupTimeLabel->setText(tr("There is no backup yet."));
        ui->backupTimeLabelExtra->setVisible(false);
    }
    else
    {
        QString extra = intervalToString(m_lastPerformedBackupTimeMs);
        ui->backupTimeLabel->setText(tr("Archive backup is completed up to <b>%1</b>").
            arg(backupPositionToString(m_lastPerformedBackupTimeMs)));
        ui->backupTimeLabelExtra->setVisible(!extra.isEmpty());
        ui->backupTimeLabelExtra->setText(lit("(%1)").arg(extra));
    }

    if (m_backupSchedule.backupType == vms::api::BackupType::scheduled)
    {
        if (m_nextScheduledBackupTimeMs)
        {
            QString extra = intervalToString(m_nextScheduledBackupTimeMs);
            ui->backupScheduleLabel->setText(tr("Next backup is scheduled for <b>%1</b>").
                arg(backupPositionToString(m_nextScheduledBackupTimeMs)));
            ui->backupScheduleLabelExtra->setVisible(!extra.isEmpty());
            ui->backupScheduleLabelExtra->setText(lit("(%1)").arg(extra));
        }
        else
        {
            ui->backupScheduleLabel->setText(setWarningStyleHtml(tr("Next backup is not scheduled.")));
            ui->backupScheduleLabelExtra->setVisible(false);
        }
    }
}

void QnStorageConfigWidget::updateCamerasForBackup(const QnVirtualCameraResourceList& cameras)
{
    if (m_camerasToBackup.toSet() == cameras.toSet())
        return;

    m_camerasToBackup = cameras;

    updateBackupInfo();
    emit hasChangesChanged();
}

void QnStorageConfigWidget::applyCamerasToBackup(const QnVirtualCameraResourceList& cameras,
    nx::vms::api::CameraBackupQualities quality)
{
    qnGlobalSettings->setBackupQualities(quality);
    qnGlobalSettings->setBackupNewCamerasByDefault(m_backupNewCameras);
    qnGlobalSettings->synchronizeNow();

    const auto qualityForCamera = [cameras, quality](const QnVirtualCameraResourcePtr& camera)
    {
        return (cameras.contains(camera) ? quality : nx::vms::api::CameraBackupQuality::CameraBackup_Disabled);
    };

    /* Update all default cameras and all cameras that we have changed. */
    const auto modifiedFilter = [qualityForCamera](const QnVirtualCameraResourcePtr& camera)
    {
        return camera->getBackupQualities() != qualityForCamera(camera);
    };

    const auto modified = resourcePool()->getAllCameras(QnResourcePtr(), true).filtered(modifiedFilter);

    if (modified.isEmpty())
        return;

    qnResourcesChangesManager->saveCameras(modified,
        [qualityForCamera](const QnVirtualCameraResourcePtr& camera)
        {
            camera->setBackupQualities(qualityForCamera(camera));
        });
}

void QnStorageConfigWidget::updateRebuildUi(QnServerStoragesPool pool, const QnStorageScanData& reply)
{
    m_model->updateRebuildInfo(pool, reply);

    bool isMainPool = pool == QnServerStoragesPool::Main;

    /* Here we must check actual backup schedule, not ui-selected. */
    bool backupIsInProgress = m_server
        && m_server->getBackupSchedule().backupType != vms::api::BackupType::realtime
        && qnServerStorageManager->backupStatus(m_server).state != Qn::BackupState_None;

    ui->addExtStorageToMainBtn->setEnabled(!backupIsInProgress);

    bool canStartRebuild =
            m_server
        &&  reply.state == Qn::RebuildState_None
        &&  !hasStoragesChanges(m_model->storages())
        &&  any_of(m_model->storages(), [this, isMainPool](const QnStorageModelInfo& info) {
                return info.isWritable
                    && info.isBackup != isMainPool
                    && info.isOnline
                    && m_model->storageIsActive(info);   /* Ignoring newly added external storages until Apply pressed. */
            })
        && !backupIsInProgress;

    if (isMainPool)
    {
        ui->rebuildMainWidget->loadData(reply, false);
        ui->rebuildMainButton->setEnabled(canStartRebuild);
        ui->rebuildMainButton->setVisible(reply.state == Qn::RebuildState_None);
    }
    else
    {
        ui->rebuildBackupWidget->loadData(reply, true);
        ui->rebuildBackupButton->setEnabled(canStartRebuild);
        ui->rebuildBackupButton->setVisible(reply.state == Qn::RebuildState_None);
    }
}

void QnStorageConfigWidget::at_serverRebuildStatusChanged(const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool, const QnStorageScanData& status)
{
    if (server != m_server)
        return;

    updateRebuildUi(pool, status);
    updateBackupInfo();
}

void QnStorageConfigWidget::at_serverBackupStatusChanged(const QnMediaServerResourcePtr& server,
    const QnBackupStatusData& status)
{
    if (server != m_server)
        return;

    updateBackupUi(status, m_camerasToBackup.size());
    updateRebuildInfo();
}

void QnStorageConfigWidget::at_serverRebuildArchiveFinished(const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool)
{
    if (server != m_server)
        return;

    if (!isVisible())
        return;

    bool isMain = (pool == QnServerStoragesPool::Main);
    StoragePool& storagePool = (isMain ? m_mainPool : m_backupPool);

    if (!storagePool.rebuildCancelled)
    {
        const auto text = (isMain
            ? tr("Archive reindexing completed")
            : tr("Backup reindexing completed"));

        QnMessageBox::success(this, text);
    }

    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::at_serverBackupFinished(const QnMediaServerResourcePtr& server)
{
    if (server != m_server)
        return;

    if (!isVisible())
        return;

    if (m_backupCancelled)
    {
        m_backupCancelled = false;
        return;
    }

    QnMessageBox::success(this, tr("Backup completed"));
}

void QnStorageConfigWidget::invokeBackupSettings()
{
    QScopedPointer<QnBackupSettingsDialog> settingsDialog(new QnBackupSettingsDialog(this));

    settingsDialog->setQualities(m_quality);
    settingsDialog->setSchedule(m_backupSchedule);
    settingsDialog->setCamerasToBackup(m_camerasToBackup);
    settingsDialog->setBackupNewCameras(m_backupNewCameras);
    settingsDialog->setReadOnly(isReadOnly());

    if (settingsDialog->exec() != QDialog::Accepted || isReadOnly())
        return;

    m_quality = settingsDialog->qualities();
    m_backupSchedule = settingsDialog->schedule();
    m_camerasToBackup = settingsDialog->camerasToBackup();
    m_backupNewCameras = settingsDialog->backupNewCameras();

    emit hasChangesChanged();
}
