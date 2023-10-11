// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_config_widget.h"
#include "ui_storage_config_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

#include <analytics/db/analytics_db_types.h>
#include <api/model/storage_status_reply.h>
#include <api/server_rest_connection.h>
#include <common/common_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_scan_info.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/settings/system_settings_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/delegates/switch_item_delegate.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/time/formatter.h>
#include <server/server_storage_manager.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/models/storage_list_model.h>
#include <ui/models/storage_model_info.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>

using namespace nx;
using namespace nx::vms::client::desktop;

namespace {

static constexpr int kMinimumColumnWidth = 80;

NX_REFLECTION_ENUM_CLASS(StorageType,
    ram,
    local,
    usb,
    network,
    smb,
    cloud,
    optical,
    swap,
    unknown
);

enum class StoragePoolMenuItem
{
    main,
    backup,
};

static int storageTypeInfo(const QnStorageModelInfo& storageData)
{
    StorageType storageType = StorageType::unknown;
    nx::reflect::fromString(storageData.storageType.toStdString(), &storageType);
    return (int) storageType;
}

static bool strLessThan(const QString& strLeft, const QString& strRight)
{
    for (int i = 0, count = qMin(strLeft.length(), strRight.length()); i < count; ++i)
    {
        const QChar left = strLeft[i];
        const QChar right = strRight[i];

        if (left != right)
        {
            return left.toLower() < right.toLower()
                || (left.toLower() == right.toLower() && left < right);
        }
    }

    return strLeft.length() < strRight.length();
}

bool backupSettingsCompatibleForCloudBackup(const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::vms::api;

    NX_ASSERT(backup_settings_view::isBackupSupported(camera));

    return !camera->getBackupContentType().testFlag(BackupContentType::archive);
}

void fixupBackupSettingsForForCloudBackup(const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::vms::api;

    if (!NX_ASSERT(backup_settings_view::isBackupSupported(camera)))
        return;

    if (!camera->getBackupContentType().testFlag(BackupContentType::archive))
        return;

    camera->setBackupContentType(
        {BackupContentType::analytics,
         BackupContentType::motion,
         BackupContentType::bookmarks});
}

QnVirtualCameraResourceList getCamerasWithIncompatibleCloudBackupSettings(
    const QnMediaServerResourcePtr& server)
{
    const auto allCameras = server->resourcePool()->getAllCameras(
        server->getId(),
        /*ignoreDesktopCameras*/ true);

    return allCameras.filtered(
        [](const auto& camera)
        {
            // TODO: #vbreus Better move 'isBackupSupported' function to the another module.
            // Despite the fact that it's trivial and have single line check it's good to have
            // single implementation of it the common module
            return backup_settings_view::isBackupSupported(camera)
                && !backupSettingsCompatibleForCloudBackup(camera);
        });
}

//-------------------------------------------------------------------------------------------------
// StoragesSortModel

class StoragesSortModel: public QSortFilterProxyModel
{
public:
    StoragesSortModel(QObject* parent = nullptr):
        QSortFilterProxyModel(parent)
    {
    }

protected:
    virtual bool lessThan(
        const QModelIndex& leftIndex,
        const QModelIndex& rightIndex) const override
    {
        auto left = leftIndex.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();
        auto right = rightIndex.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();
        int groupOrderLeft = storageTypeInfo(left);
        int groupOrderRight = storageTypeInfo(right);

        return groupOrderLeft < groupOrderRight ||
            (groupOrderLeft == groupOrderRight && strLessThan(left.url, right.url));
    }
};

//-------------------------------------------------------------------------------------------------
// StorageTableItemDelegate

class StorageTableItemDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    StorageTableItemDelegate(ItemViewHoverTracker* hoverTracker, QObject* parent = nullptr):
        base_type(parent),
        m_hoverTracker(hoverTracker)
    {
    }

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
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

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        bool editableColumn = opt.state.testFlag(QStyle::State_Enabled)
            && index.column() == QnStorageListModel::StoragePoolColumn
            && index.flags().testFlag(Qt::ItemIsEditable);

        bool hovered = m_hoverTracker && m_hoverTracker->hoveredIndex() == index;
        bool beingEdited = m_editedRow == index.row();

        bool hoveredRow = m_hoverTracker && m_hoverTracker->hoveredIndex().row() == index.row();
        bool hasHoverableText = index.data(QnStorageListModel::ShowTextOnHoverRole).toBool();

        // Hide actions when their row is not hovered.
        if (hasHoverableText && !hoveredRow)
            return;

        auto storage = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

        // Set disabled style for unchecked rows.
        if (!index.sibling(index.row(), QnStorageListModel::CheckBoxColumn).data(Qt::CheckStateRole).toBool())
            opt.state &= ~QStyle::State_Enabled;

        // Set proper color for action text buttons.
        if (index.column() == QnStorageListModel::ActionsColumn)
        {
            if (hasHoverableText && hovered)
                opt.palette.setColor(QPalette::Text, vms::client::core::colorTheme()->color("light14"));
            else if (hasHoverableText)
                opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::WindowText));
            else // Either hidden (has no text) or selected, we can use 'Selected' style for both.
                opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Light));
        }

        // Set warning color for inaccessible storages.
        if (index.column() == QnStorageListModel::StoragePoolColumn && !storage.isOnline)
            opt.palette.setColor(QPalette::Text, vms::client::core::colorTheme()->color("red_l2"));

        // Set proper color for hovered storage type column.
        if (!opt.state.testFlag(QStyle::State_Enabled))
            opt.palette.setCurrentColorGroup(QPalette::Disabled);
        if (editableColumn && hovered)
            opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::ButtonText));

        // Draw item.
        QStyle* style = option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

        // Draw arrow if editable storage type column.
        if (editableColumn)
        {
            QStyleOption arrowOption = opt;
            arrowOption.rect.setLeft(arrowOption.rect.left() +
                style::Metrics::kStandardPadding +
                opt.fontMetrics.horizontalAdvance(opt.text) +
                style::Metrics::kArrowSize / 2);

            arrowOption.rect.setWidth(style::Metrics::kArrowSize);

            auto arrow = beingEdited
                ? QStyle::PE_IndicatorArrowUp
                : QStyle::PE_IndicatorArrowDown;

            QStyle* style = option.widget ? option.widget->style() : QApplication::style();
            style->drawPrimitive(arrow, &arrowOption, painter);
        }
    }

    virtual QWidget* createEditor(
        QWidget*,
        const QStyleOptionViewItem&,
        const QModelIndex&) const override
    {
        return nullptr;
    }

    void setEditedRow(int row)
    {
        m_editedRow = row;
    }

private:
    QPointer<ItemViewHoverTracker> m_hoverTracker;
    int m_editedRow = -1;
};

//-------------------------------------------------------------------------------------------------
// ColumnResizer

class ColumnResizer: public QObject
{
public:
    ColumnResizer(QObject* parent = nullptr):
        QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject* object, QEvent* event)
    {
        auto view = qobject_cast<TreeView*>(object);

        if (view && event->type() == QEvent::Resize)
        {
            int occupiedWidth = 0;
            for (int i = 1; i < view->model()->columnCount(); ++i)
                occupiedWidth += view->sizeHintForColumn(i);

            const int urlWidth = view->sizeHintForColumn(QnStorageListModel::UrlColumn);
            view->setColumnWidth(QnStorageListModel::UrlColumn,
                qMin(urlWidth, view->width() - occupiedWidth));
        }

        return false;
    }
};

static constexpr int kUpdateStatusTimeoutMs = 5 * 1000;

} // namespace

//-------------------------------------------------------------------------------------------------
// MetadataWatcher

class QnStorageConfigWidget::MetadataWatcher:
    public QObject,
    public SystemContextAware
{
    const std::chrono::milliseconds kUpdateDelay = std::chrono::milliseconds(100);

public:
    MetadataWatcher(SystemContext* systemContext, QObject* parent = nullptr):
        QObject(parent),
        SystemContextAware(systemContext),
        m_updateEngines([this]{ updateEnginesFromServer(); }, kUpdateDelay, this)
    {
        m_updateEngines.setFlags(utils::PendingOperation::FireOnlyWhenIdle);

        connect(resourcePool(), &QnResourcePool::resourceAdded,
            this, &MetadataWatcher::handleResourceAdded);

        connect(resourcePool(), &QnResourcePool::resourceRemoved,
            this, &MetadataWatcher::handleResourceRemoved);

        for (const auto& camera: resourcePool()->getAllCameras())
            handleResourceAdded(camera);

        connect(
            systemContext->serverRuntimeEventConnector(),
            &ServerRuntimeEventConnector::analyticsStorageParametersChanged,
            this,
            [this](const QnUuid& serverId)
            {
                if (m_server && serverId == m_server->getId())
                    requestMetadataFromServer();
            });
    }

    bool metadataMayExist() const
    {
        return m_server && !m_server->metadataStorageId().isNull()
            && (m_metadataExists || m_enginesEnabled);
    }

    void setServer(const QnMediaServerResourcePtr& server)
    {
        if (m_server == server)
            return;

        if (m_server)
            m_server->disconnect(this);

        m_server = server;
        if (m_server.isNull())
            return;

        // Check if we have enabled analytics engines.
        m_enginesEnabled = nx::analytics::serverHasActiveObjectEngines(m_server);

        connect(m_server.get(), &QnResource::propertyChanged, this,
            [this](const QnResourcePtr&, const QString& key)
            {
                if (key == ResourcePropertyKey::Server::kMetadataStorageIdKey)
                {
                    // Assume that we have metadata while the database is being loaded.
                    m_metadataExists = true;
                }
            });

        requestMetadataFromServer();
    }

    void requestMetadataFromServer()
    {
        if (!m_server)
        {
            m_metadataExists = false;
            return;
        }

        // Assume that we have stored data until we receive response from server.
        m_metadataExists = true;

        nx::analytics::db::Filter filter;
        filter.maxObjectTracksToSelect = 1;
        filter.timePeriod = {QnTimePeriod::kMinTimeValue, QnTimePeriod::kMaxTimeValue};

        auto callback = utils::guarded(this,
            [this, originalServer = m_server->getId()](
                bool success,
                rest::Handle,
                nx::analytics::db::LookupResult&& result)
            {
                if (success && m_server && originalServer == m_server->getId())
                    m_metadataExists = !result.empty();
            });

        if (!NX_ASSERT(connection()))
            return;

        connectedServerApi()->lookupObjectTracks(
            filter,
            /*isLocal*/ true,
            callback,
            this->thread(),
            m_server->getId());
    }

    // This method is called for the newly added cameras.
    // May only set m_enginesEnabled to true,.
    void updateEnginesFromNewCamera(const QnVirtualCameraResourcePtr& camera)
    {
        if (m_enginesEnabled)
            return;

        if (!m_server || camera->getParentId() != m_server->getId())
            return;

        m_enginesEnabled = !camera->supportedObjectTypes().empty();
    }

    // This method is called when analytics is enabled or disabled on some camera.
    // May set m_enginesEnabled to true, or schedule its recalculation.
    void updateEnginesFromExistingCamera(const QnVirtualCameraResourcePtr& camera)
    {
        if (!m_server || camera->getParentId() != m_server->getId())
            return;

        bool cameraEnginesEnabled = !camera->supportedObjectTypes().empty();

        if (m_enginesEnabled != cameraEnginesEnabled)
        {
            if (!cameraEnginesEnabled) // Perhaps that was the last camera with enabled analytics.
                m_updateEngines.requestOperation();
            else
                m_enginesEnabled = true;
        }
    }

    // Checks if the current server has enabled analytics, stores the result in m_enginesEnabled.
    // That's the only place except setServer() where m_enginesEnabled may turn from true, to false.
    // This method should not be called directly, it's wrapped by m_updateEngines PendingOperation,
    // which avoids unnecessary calls of this method in the case of mass camera deletion.
    void updateEnginesFromServer()
    {
        if (!m_server)
            return;

        const bool enginesEnabled = nx::analytics::serverHasActiveObjectEngines(m_server);

        if (m_enginesEnabled && !enginesEnabled)
        {
            // It's possible that user has enabled and than disabled analytics engines.
            // We need to check if some metadata was stored during this period.
            requestMetadataFromServer();
        }

        m_enginesEnabled = enginesEnabled;
    }

    void handleResourceAdded(const QnResourcePtr& resource)
    {
        if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
        {
            connect(camera.get(), &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
                this, &MetadataWatcher::updateEnginesFromExistingCamera);

            connect(camera.get(), &QnVirtualCameraResource::parentIdChanged, this,
                [this]{ m_updateEngines.requestOperation(); });

            updateEnginesFromNewCamera(camera);
        }
    }

    void handleResourceRemoved(const QnResourcePtr& resource)
    {
        if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
        {
            camera->disconnect(this);
            m_updateEngines.requestOperation();
        }
    }

private:
    QnMediaServerResourcePtr m_server;
    bool m_enginesEnabled = false;
    bool m_metadataExists = false;
    nx::utils::PendingOperation m_updateEngines;
};

//-------------------------------------------------------------------------------------------------
// QnStorageConfigWidget

QnStorageConfigWidget::QnStorageConfigWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageConfigWidget()),
    m_server(),
    m_model(new QnStorageListModel()),
    m_columnResizer(new ColumnResizer(this)),
    m_updateStatusTimer(new QTimer(this)),
    m_storagePoolMenu(new QMenu(this))
{
    ui->setupUi(this);
    connect(m_model.get(), &QnStorageListModel::dataChanged, [this] { updateWarnings(); });

    m_storagePoolMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    const auto mainAction = m_storagePoolMenu->addAction(tr("Main"));
    const auto backupAction = m_storagePoolMenu->addAction(tr("Backup"));

    mainAction->setData(static_cast<int>(StoragePoolMenuItem::main));
    backupAction->setData(static_cast<int>(StoragePoolMenuItem::backup));

    setHelpTopic(this, HelpTopic::Id::ServerSettings_Storages);

    ui->rebuildBackupButtonHint->addHintLine(tr("Reindexing can fix problems with archive or "
        "backup if they have been lost or damaged, or if some hardware has been replaced."));
    setHelpTopic(ui->rebuildBackupButtonHint, HelpTopic::Id::ServerSettings_ArchiveRestoring);

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
    ui->storageView->header()->setSectionResizeMode(
        QnStorageListModel::SeparatorColumn, QHeaderView::Stretch);
    ui->storageView->header()->setSectionsMovable(false);

    ui->storageView->setMouseTracking(true);
    ui->storageView->installEventFilter(m_columnResizer.data());

    auto itemClicked =
        [this, itemDelegate](const QModelIndex& index)
        {
            itemDelegate->setEditedRow(index.row());
            ui->storageView->update(index);
            atStorageViewClicked(index);
            itemDelegate->setEditedRow(-1);
            ui->storageView->update(index);
        };

    connect(ui->storageView, &TreeView::clicked, this, itemClicked);

    connect(ui->addExtStorageToMainBtn, &QPushButton::clicked, this,
        [this]() { atAddExtStorage(true); });

    connect(ui->rebuildMainButton, &QPushButton::clicked, this,
        [this] { startRebuid(true); });

    connect(ui->rebuildBackupButton, &QPushButton::clicked, this,
        [this] { startRebuid(false); });

    connect(ui->rebuildMainWidget, &QnStorageRebuildWidget::cancelRequested, this,
        [this]{ cancelRebuild(true); });

    connect(ui->rebuildBackupWidget, &QnStorageRebuildWidget::cancelRequested, this,
        [this]{ cancelRebuild(false); });

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildStatusChanged,
        this, &QnStorageConfigWidget::atServerRebuildStatusChanged);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished,
        this, &QnStorageConfigWidget::atServerRebuildArchiveFinished);

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
            if (!m_updating)
                updateRebuildInfo();
        });

    m_updateStatusTimer->setInterval(kUpdateStatusTimeoutMs);
    connect(m_updateStatusTimer, &QTimer::timeout, this,
        [this]
        {
            if (isVisible())
                qnServerStorageManager->checkStoragesStatus(m_server);
        });

    /* By [Left] disable storage, by [Right] enable storage: */
    installEventHandler(ui->storageView, QEvent::KeyPress, this,
        [this, itemClicked](QObject*, QEvent* event)
        {
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

    m_metadataWatcher.reset(new MetadataWatcher(systemContext(), this));
}

QnStorageConfigWidget::~QnStorageConfigWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void QnStorageConfigWidget::setReadOnlyInternal(bool readOnly)
{
    m_model->setReadOnly(readOnly);
    // TODO: #sivanov Handle safe mode.
}

bool QnStorageConfigWidget::isNetworkRequestRunning() const
{
    return m_currentRequest != 0;
}

void QnStorageConfigWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_updateStatusTimer->start();
    qnServerStorageManager->checkStoragesStatus(m_server);
}

void QnStorageConfigWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    m_updateStatusTimer->stop();
}

QnStorageConfigWidget::StorageConfigWarningFlags
    QnStorageConfigWidget::storageConfigurationWarningFlags() const
{
    StorageConfigWarningFlags flags;

    for (const auto& storageInfo: m_model->storages())
    {
        const auto storageResource =
            resourcePool()->getResourceById<QnStorageResource>(storageInfo.id);

        if (!storageResource || storageResource->getParentId() != m_server->getId())
            continue;

        // Metadata related flags.
        if (storageInfo.id == m_model->metadataStorageId())
        {
            const auto isSystemStorage = storageResource->persistentStatusFlags()
                .testFlag(vms::api::StoragePersistentFlag::system);

            if (isSystemStorage)
                flags.setFlag(analyticsIsOnSystemDrive);

            if (!storageInfo.isUsed)
                flags.setFlag(analyticsIsOnDisabledStorage);
        }

        // Storage enabled state wasn's affected, skipping further checks.
        if (storageInfo.isUsed == storageResource->isUsedForWriting())
            continue;

        // TODO: Get rid of serialized literals, use PartitionType enum value instead.

        // Storages status and configuratiom messages.
        if (!storageInfo.isUsed)
            flags.setFlag(hasDisabledStorage);

        if (storageInfo.isUsed && storageInfo.storageType == "usb")
            flags.setFlag(hasUsbStorage);

        // Cloud backup storage related flags.
        if (storageInfo.isUsed && storageInfo.storageType == "cloud")
            flags.setFlag(cloudBackupStorageBeingEnabled);

        if (!storageInfo.isUsed && storageResource->isUsedForWriting() && storageResource->isBackup())
            flags.setFlag(backupStorageBeingReplacedByCloudStorage);
    }

    return flags;
}

void QnStorageConfigWidget::updateWarnings()
{
    QScopedValueRollback<bool> updatingGuard(m_updating, true);

    const auto flags = storageConfigurationWarningFlags();

    std::vector<BarDescription> messages;

    if (flags.testFlag(analyticsIsOnSystemDrive))
    {
        messages.push_back(
            {
                .text = tr("Analytics data can take up large amounts of space. We recommend "
                    "choosing another location for it instead of the system partition."),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty =
                    &messageBarSettings()->storageConfigAnalyticsOnSystemStorageWarning
            });
    }
    if (flags.testFlag(analyticsIsOnDisabledStorage))
    {
        messages.push_back(
            {
                .text = tr("Analytics and motion data will continue to be stored on the "
                    "disabled storage"),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty =
                    &messageBarSettings()->storageConfigAnalyticsOnDisabledStorageWarning
            });
    }
    if (flags.testFlag(hasDisabledStorage))
    {
        messages.push_back(
            {
                .text = tr("Recording to disabled storage location will stop. However,"
                    " deleting outdated footage from it will continue."),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->storageConfigHasDisabledWarning
            });
    }
    if (flags.testFlag(hasUsbStorage))
    {
        messages.push_back(
            {
                .text = tr("Recording was enabled on the USB storage"),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->storageConfigUsbWarning
            });
    }
    if (flags.testFlags({cloudBackupStorageBeingEnabled, backupStorageBeingReplacedByCloudStorage}))
    {
        messages.push_back(
            {
                .text = tr("If cloud storage is activated for backup, other backup storages will "
                    "be deactivated and the “All archive” option for already configured devices "
                    "will be changed to “Motion, Object, Bookmarks”"),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->storageConfigCloudStorageWarning
            });
    }
    else if (flags.testFlag(cloudBackupStorageBeingEnabled))
    {
        messages.push_back(
            {
                .text = tr("If cloud storage is activated for backup, the “All archive” option "
                    "for already configured devices will be changed to “Motion, Object, Bookmarks”"),
                .level = BarDescription::BarLevel::Warning,
                .isEnabledProperty = &messageBarSettings()->storageConfigCloudStorageWarning
            });
    }
    ui->messageBarBlock->setMessageBars(messages);
}

bool QnStorageConfigWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return hasStoragesChanges(m_model->storages());
}

void QnStorageConfigWidget::atAddExtStorage(bool addToMain)
{
    if (!m_server || isReadOnly())
        return;

    auto storageManager = qnServerStorageManager;
    QScopedPointer<QnStorageUrlDialog> dialog(new QnStorageUrlDialog(m_server, storageManager, this));
    QSet<QString> protocols;
    for (const auto& p: storageManager->protocols(m_server))
        protocols.insert(QString::fromStdString(p));
    dialog->setProtocols(protocols);
    dialog->setCurrentServerStorages(m_model->storages());
    if (!dialog->exec())
        return;

    QnStorageModelInfo item = dialog->storage();
    // Check if somebody have added this storage right now.
    if (item.id.isNull())
    {
        // New storages has tested and ready to add
        const auto credentials = dialog->credentials();
        QUrl url(item.url);
        if (!url.scheme().isEmpty() && !credentials.isEmpty())
        {
            url.setUserName(credentials.user);
            url.setPassword(credentials.password);
            item.url = url.toString();
        }
        item.id = QnStorageResource::fillID(m_server->getId(), item.url);
        item.isBackup = !addToMain;
        item.isUsed = true;
        item.isOnline = true;
    }

    m_model->addStorage(item); //< Adds or updates storage model data.
    emit hasChangesChanged();
}

bool QnStorageConfigWidget::cloudStorageToggledOn() const
{
    const auto cloudStorage = m_model->cloudBackupStorage();

    if (!cloudStorage)
        return false;

    if (!cloudStorage->isUsed)
        return false;

    const auto storageResource =
        resourcePool()->getResourceById<QnStorageResource>(cloudStorage->id);

    return !storageResource->isUsedForWriting();
}

void QnStorageConfigWidget::updateBackupSettingsForCloudStorage()
{
    const auto cameras = getCamerasWithIncompatibleCloudBackupSettings(m_server);
    if (cameras.empty())
        return;

    const auto applyChanges =
        [cameras]
        {
            for (const auto& camera: cameras)
                fixupBackupSettingsForForCloudBackup(camera);
        };

    // TODO: #vbreus Implement saving via REST, add returned handle to requests list.

    // TODO: #vbreus This is not really correct way to alter backup settings, changes done on
    // 'Backup' tab should be merged with this ones and applied via single request.

    // TODO: #vbreus Move applied backup settings to the ServerSettingsDialogState.

    qnResourcesChangesManager->saveCamerasBatch(cameras, applyChanges);
}

void QnStorageConfigWidget::loadDataToUi()
{
    if (!m_server)
        return;

    loadStoragesFromResources();
    updateWarnings();
    updateRebuildInfo();
}

void QnStorageConfigWidget::loadStoragesFromResources()
{
    NX_ASSERT(m_server, "Server must exist here");

    QnStorageModelInfoList storages;
    for (const QnStorageResourcePtr& storage: m_server->getStorages())
        storages.append(QnStorageModelInfo(storage));

    m_model->setStorages(storages);
}

void QnStorageConfigWidget::atStorageViewClicked(const QModelIndex& index)
{
    if (!m_server || isReadOnly())
        return;

    QnStorageModelInfo record = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

    if (index.column() == QnStorageListModel::StoragePoolColumn)
    {
        if (!m_model->canChangeStoragePool(record)
            || !index.flags().testFlag(Qt::ItemIsEditable)
            || !index.flags().testFlag(Qt::ItemIsUserCheckable))
        {
            return;
        }

        auto findAction =
            [this](StoragePoolMenuItem menuItem) -> QAction*
            {
                for (auto action: m_storagePoolMenu->actions())
                {
                    const auto actionData = action->data();
                    if (actionData.isNull())
                        continue;

                    StoragePoolMenuItem actionItem =
                        static_cast<StoragePoolMenuItem>(actionData.toInt());

                    if (menuItem == actionItem)
                        return action;
                }

                NX_ASSERT(false, "Unexpected action type passed as parameter");
                return nullptr;
            };

        const auto activeAction = findAction(record.isBackup
            ? StoragePoolMenuItem::backup
            : StoragePoolMenuItem::main);

        m_storagePoolMenu->setActiveAction(activeAction);

        // Disable "Backup" menu option if cloud is used for backup, otherwise storage will be
        // disabled and become non-interactive.
        const auto cloudStorageInfo = m_model->cloudBackupStorage();
        const auto backupMenuActionEnabled = !cloudStorageInfo || !cloudStorageInfo->isUsed;
        findAction(StoragePoolMenuItem::backup)->setEnabled(backupMenuActionEnabled);

        QPoint point = ui->storageView->viewport()->mapToGlobal(
            ui->storageView->visualRect(index).bottomLeft()) + QPoint(0, 1);

        auto selection = m_storagePoolMenu->exec(point);
        if (!selection)
            return;

        const auto selectedMenuItem = static_cast<StoragePoolMenuItem>(selection->data().toInt());

        switch (selectedMenuItem)
        {
            case StoragePoolMenuItem::backup:
                if (!backupMenuActionEnabled || record.isBackup)
                    return;

                record.isBackup = true;
                break;

            case StoragePoolMenuItem::main:
                if (!record.isBackup)
                    return;

                record.isBackup = false;
                break;

            default:
                NX_ASSERT(false, "Unexpected action type picked from context menu");
                return;
        }

        m_model->updateStorage(record);
    }
    else if (index.column() == QnStorageListModel::ActionsColumn)
    {
        if (m_model->canRemoveStorage(record))
        {
            // Network storage.
            m_model->removeStorage(record);
        }
        else if (m_model->couldStoreAnalytics(record))
        {
            const auto storageId = record.id;
            if (storageId != m_model->metadataStorageId())
            {
                // Check storage access rights.
                if (record.isDbReady || globalSettings()->forceAnalyticsDbStoragePermissions())
                {
                    confirmNewMetadataStorage(storageId);
                }
                else
                {
                    QnMessageBox::critical(
                        this, tr("Insufficient permissions to store analytics data."));
                }
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
    m_metadataWatcher->setServer(server);
    m_model->setServer(server);

    if (m_server)
    {
        connect(m_server.get(), &QnMediaServerResource::propertyChanged, this,
            [this](const QnResourcePtr&, const QString& key)
            {
                // Now metadata storage is changed instantly, so we don't fire hasChangedChanged().
                if (key == ResourcePropertyKey::Server::kMetadataStorageIdKey)
                    m_model->setMetadataStorageId(m_server->metadataStorageId());
            });

        connect(m_server.get(), &QnResource::statusChanged,
            this, &QnStorageConfigWidget::updateRebuildInfo);
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

void QnStorageConfigWidget::applyStoragesChanges(
    QnStorageResourceList& result,
    const QnStorageModelInfoList& storages) const
{
    auto existing = m_server->getStorages();
    for (const auto& storageData: storages)
    {
        auto existingIt = std::find_if(existing.cbegin(), existing.cend(),
            [&storageData](const auto& storage) { return storageData.url == storage->getUrl(); });

        if (existingIt != existing.cend())
        {
            auto storage = *existingIt;
            if (storageData.isUsed != storage->isUsedForWriting()
                || storageData.isBackup != storage->isBackup())
            {
                storage->setUsedForWriting(storageData.isUsed);
                storage->setBackup(storageData.isBackup);
                result.push_back(storage);
            }
        }
        else
        {
            QnClientStorageResourcePtr storage =
                QnClientStorageResource::newStorage(m_server, storageData.url);
            NX_ASSERT(storage->getId() == storageData.id, "Id's must be equal");

            storage->setUsedForWriting(storageData.isUsed);
            storage->setStorageType(storageData.storageType);
            storage->setBackup(storageData.isBackup);
            storage->setSpaceLimit(storageData.reservedSpace);

            resourcePool()->addResource(storage);
            result.push_back(storage);
        }
    }
}

bool QnStorageConfigWidget::hasStoragesChanges(const QnStorageModelInfoList& storages) const
{
    if (!m_server)
        return false;

    for (const auto& storageData: storages)
    {
        // New storage was added.
        QnStorageResourcePtr storage =
            resourcePool()->getResourceById<QnStorageResource>(storageData.id);

        if (!storage || storage->getParentId() != m_server->getId())
            return true;

        // Storage was modified.
        if (storageData.isUsed != storage->isUsedForWriting()
            || storageData.isBackup != storage->isBackup())
        {
            return true;
        }
    }

    // Storage was removed.
    auto existingStorages = m_server->getStorages();
    return storages.size() != existingStorages.size();
}

bool QnStorageConfigWidget::isServerOnline() const
{
    return m_server && m_server->getStatus() == nx::vms::api::ResourceStatus::online;
}

void QnStorageConfigWidget::applyChanges()
{
    if (isReadOnly())
        return;

    QnStorageResourceList storagesToUpdate;
    vms::api::IdDataList storagesToRemove;

    if (cloudStorageToggledOn())
        updateBackupSettingsForCloudStorage();

    applyStoragesChanges(storagesToUpdate, m_model->storages());

    QSet<QnUuid> newIdList;
    for (const auto& storageData: m_model->storages())
        newIdList.insert(storageData.id);

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

    updateWarnings();
    emit hasChangesChanged();
}

void QnStorageConfigWidget::discardChanges()
{
    if (auto api = connectedServerApi(); api && m_currentRequest > 0)
        api->cancelRequest(m_currentRequest);
    m_currentRequest = 0;
}

void QnStorageConfigWidget::startRebuid(bool isMain)
{
    if (!m_server)
        return;

    const auto extras =
        tr("Depending on the total size of the archive, reindexing can take up to several hours.")
        + '\n' +tr("Reindexing is only necessary if your archive folders have been moved, renamed or deleted.")
        + '\n' +tr("You can cancel this operation at any moment without data loss.")
        + '\n' +tr("Continue anyway?");

    const auto warnResult = QnMessageBox::warning(this,
        tr("Hard disk load will increase significantly"), extras,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Ok);

    if (warnResult != QDialogButtonBox::Ok)
        return;

    if (!qnServerStorageManager->rebuildServerStorages(m_server,
        isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
    {
        return;
    }

    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    storagePool.rebuildCancelled = false;
}

void QnStorageConfigWidget::cancelRebuild(bool isMain)
{
    if (!qnServerStorageManager->cancelServerStoragesRebuild(m_server,
        isMain ? QnServerStoragesPool::Main : QnServerStoragesPool::Backup))
    {
        return;
    }

    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;
    storagePool.rebuildCancelled = true;
}

void QnStorageConfigWidget::confirmNewMetadataStorage(const QnUuid& storageId)
{
    const auto updateServerSettings =
        [this](const vms::api::MetadataStorageChangePolicy policy, const QnUuid& storageId)
        {
            NX_ASSERT(m_currentRequest == 0);

            systemContext()->systemSettings()->metadataStorageChangePolicy = policy;

            const auto callback =
                [this, storageId](bool success, rest::Handle requestId)
                {
                    NX_ASSERT(m_currentRequest == requestId || m_currentRequest == 0);
                    m_currentRequest = 0;
                    if (!success)
                        return;

                    auto internalCallback = nx::utils::guarded(this,
                        [this](bool /*success*/, rest::Handle requestId)
                        {
                            NX_ASSERT(m_currentRequest == requestId || m_currentRequest == 0);
                            m_currentRequest = 0;
                        });

                    m_server->setMetadataStorageId(storageId);
                    m_currentRequest = qnResourcesChangesManager->saveServer(
                        m_server,
                        internalCallback);
                };

            m_currentRequest = systemContext()->systemSettingsManager()->saveSystemSettings(
                callback,
                this);
        };

    if (m_metadataWatcher->metadataMayExist())
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
            updateServerSettings(vms::api::MetadataStorageChangePolicy::remove, storageId);
            updateWarnings();
        }

        if (msgBox.clickedButton() == keepButton)
        {
            updateServerSettings(vms::api::MetadataStorageChangePolicy::keep, storageId);
            updateWarnings();
        }

        // If the cancel button has been clicked, do nothing...
    }
    else
    {
        // We don't have analytics database yet, so just assign a new storage.
        updateServerSettings(vms::api::MetadataStorageChangePolicy::keep, storageId); //< Just to be sure...
        updateWarnings();
    }
}

void QnStorageConfigWidget::updateRebuildUi(
    QnServerStoragesPool pool,
    const nx::vms::api::StorageScanInfo& reply)
{
    QScopedValueRollback<bool> updatingGuard(m_updating, true);

    m_model->updateRebuildInfo(pool, reply);

    const bool isMainPool = pool == QnServerStoragesPool::Main;

    ui->addExtStorageToMainBtn->setEnabled(isServerOnline());
    const auto modelStorages = m_model->storages();

    const bool canStartRebuild =
        isServerOnline()
        && reply.state == nx::vms::api::RebuildState::none
        && !hasStoragesChanges(m_model->storages())
        && std::any_of(modelStorages.cbegin(), modelStorages.cend(),
            [this, isMainPool](const QnStorageModelInfo& info)
            {
                return info.isWritable
                    && info.isBackup != isMainPool
                    && info.isOnline
                    && m_model->storageIsActive(info); //< Ignoring newly added external storages until Apply pressed.
            });

    if (isMainPool)
    {
        ui->rebuildMainWidget->loadData(reply, false);
        ui->rebuildMainButton->setEnabled(canStartRebuild);
        ui->rebuildMainButton->setVisible(reply.state == nx::vms::api::RebuildState::none);
    }
    else
    {
        ui->rebuildBackupWidget->loadData(reply, true);
        ui->rebuildBackupButton->setEnabled(canStartRebuild);
        ui->rebuildBackupButton->setVisible(reply.state == nx::vms::api::RebuildState::none);
    }
}

void QnStorageConfigWidget::atServerRebuildStatusChanged(
    const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool,
    const nx::vms::api::StorageScanInfo& status)
{
    if (server == m_server)
        updateRebuildUi(pool, status);
}

void QnStorageConfigWidget::atServerRebuildArchiveFinished(
    const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool)
{
    if (server != m_server)
        return;

    if (!isVisible())
        return;

    bool isMain = (pool == QnServerStoragesPool::Main);
    StoragePool& storagePool = isMain ? m_mainPool : m_backupPool;

    if (!storagePool.rebuildCancelled)
    {
        const auto text = isMain
            ? tr("Archive reindexing completed")
            : tr("Backup reindexing completed");

        QnMessageBox::success(this, text);
    }

    storagePool.rebuildCancelled = false;
}
