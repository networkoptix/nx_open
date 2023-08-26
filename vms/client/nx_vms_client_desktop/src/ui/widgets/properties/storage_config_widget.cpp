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
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/time/formatter.h>
#include <server/server_storage_manager.h>
#include <ui/dialogs/storage_url_dialog.h>
#include <ui/models/storage_list_model.h>
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
    // May only set m_enginesEnabled to true.
    void updateEnginesFromNewCamera(const QnVirtualCameraResourcePtr& camera)
    {
        if (m_enginesEnabled)
            return;

        if (!m_server || camera->getParentId() != m_server->getId())
            return;

        m_enginesEnabled = !camera->supportedObjectTypes().empty();
    }

    // This method is called when analytics is enabled or disabled on some camera.
    // May set m_enginesEnabled to true or schedule its recalculation.
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
    // That's the only place except setServer() where m_enginesEnabled may turn from true to false.
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

    m_storagePoolMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    m_storagePoolMenu->addAction(tr("Main"))->setData(false);
    m_storagePoolMenu->addAction(tr("Backup"))->setData(true);

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

    ui->storageView->setMouseTracking(true);
    ui->storageView->installEventFilter(m_columnResizer.data());

    auto itemClicked =
        [this, itemDelegate](const QModelIndex& index)
        {
            itemDelegate->setEditedRow(index.row());
            ui->storageView->update(index);
            at_storageView_clicked(index);
            itemDelegate->setEditedRow(-1);
            ui->storageView->update(index);
        };

    connect(ui->storageView, &TreeView::clicked, this, itemClicked);

    connect(ui->addExtStorageToMainBtn, &QPushButton::clicked, this,
        [this]() { at_addExtStorage(true); });

    connect(ui->rebuildMainButton, &QPushButton::clicked, this,
        [this] { startRebuid(true); });

    connect(ui->rebuildBackupButton, &QPushButton::clicked, this,
        [this] { startRebuid(false); });

    connect(ui->rebuildMainWidget, &QnStorageRebuildWidget::cancelRequested, this,
        [this]{ cancelRebuild(true); });

    connect(ui->rebuildBackupWidget, &QnStorageRebuildWidget::cancelRequested, this,
        [this]{ cancelRebuild(false); });

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildStatusChanged,
        this, &QnStorageConfigWidget::at_serverRebuildStatusChanged);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished,
        this, &QnStorageConfigWidget::at_serverRebuildArchiveFinished);

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
}

void QnStorageConfigWidget::setReadOnlyInternal(bool readOnly)
{
    m_model->setReadOnly(readOnly);
    // TODO: #sivanov Handle safe mode.
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

bool QnStorageConfigWidget::hasChanges() const
{
    if (isReadOnly())
        return false;

    return hasStoragesChanges(m_model->storages());
}

void QnStorageConfigWidget::at_addExtStorage(bool addToMain)
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

void QnStorageConfigWidget::at_storageView_clicked(const QModelIndex& index)
{
    if (!m_server || isReadOnly())
        return;

    QnStorageModelInfo record = index.data(Qn::StorageInfoDataRole).value<QnStorageModelInfo>();

    if (index.column() == QnStorageListModel::StoragePoolColumn)
    {
        if (!m_model->canChangeStoragePool(record) || !index.flags().testFlag(Qt::ItemIsEditable))
            return;

        auto findAction =
            [this](bool isBackup) -> QAction*
            {
                for (auto action: m_storagePoolMenu->actions())
                {
                    if (!action->data().isNull() && action->data().toBool() == isBackup)
                        return action;
                }
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
        else if (m_model->couldStoreAnalytics(record))
        {
            const auto storageId = record.id;
            if (storageId != m_model->metadataStorageId())
            {
                // Check storage access rights.
                if (record.isDbReady || globalSettings()->forceAnalyticsDbStoragePermissions())
                    confirmNewMetadataStorage(storageId);
                else
                    QnMessageBox::critical(this, tr("Insufficient permissions to store analytics data."));
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
            systemContext()->systemSettings()->metadataStorageChangePolicy = policy;

            const auto callback = nx::utils::guarded(this,
                [this, storageId](bool success)
                {
                    if (!success)
                        return;

                    qnResourcesChangesManager->saveServer(m_server,
                        [storageId](const auto& server)
                        {
                            server->setMetadataStorageId(storageId);
                        });
                });

            systemContext()->systemSettingsManager()->saveSystemSettings(callback, this);
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

void QnStorageConfigWidget::updateWarnings()
{
    QScopedValueRollback<bool> updatingGuard(m_updating, true);

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
            if (storage->persistentStatusFlags().testFlag(vms::api::StoragePersistentFlag::system))
                analyticsIsOnSystemDrive = true;

            if (!storageData.isUsed)
                analyticsIsOnDisabledStorage = true;
        }

        // Storage was not modified.
        if (storageData.isUsed == storage->isUsedForWriting())
            continue;

        if (!storageData.isUsed)
            hasDisabledStorage = true;

        // TODO: use PartitionType enum value here instead of the serialized literal.
        if (storageData.isUsed && storageData.storageType == lit("usb"))
            hasUsbStorage = true;
    }

    ui->analyticsOnSystemDriveWarningLabel->setVisible(analyticsIsOnSystemDrive);
    ui->analyticsOnDisabledStorageWarningLabel->setVisible(analyticsIsOnDisabledStorage);
    ui->storagesWarningLabel->setVisible(hasDisabledStorage);
    ui->usbWarningLabel->setVisible(hasUsbStorage);
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

void QnStorageConfigWidget::at_serverRebuildStatusChanged(
    const QnMediaServerResourcePtr& server,
    QnServerStoragesPool pool,
    const nx::vms::api::StorageScanInfo& status)
{
    if (server == m_server)
        updateRebuildUi(pool, status);
}

void QnStorageConfigWidget::at_serverRebuildArchiveFinished(
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
