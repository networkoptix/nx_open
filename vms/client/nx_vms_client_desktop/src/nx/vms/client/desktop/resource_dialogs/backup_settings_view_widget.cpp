// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_view_widget.h"

#include <QtCore/QPersistentModelIndex>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#include <core/resource/camera_resource.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/resource_details_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/backup_settings_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/backup_settings_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/backup_settings_picker_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/license/saas/saas_service_usage_helper.h>
#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/views/resource_list_view.h>

namespace {

using namespace std::chrono;
using namespace nx::vms::client::desktop;
using namespace backup_settings_view;

using AbstractEntityPtr = entity_item_model::AbstractEntityPtr;
using ResourceTreeEntityBuilder = entity_resource_tree::ResourceTreeEntityBuilder;

static constexpr auto kCloudStorageInfoUpdateInterval = 1min;

QModelIndexList filterSelectedIndexes(const QModelIndexList& selectedIndexes)
{
    QModelIndexList result;
    std::copy_if(selectedIndexes.cbegin(), selectedIndexes.cend(), std::back_inserter(result),
        [](const QModelIndex& index)
        {
            return index.column() == backup_settings_view::ResourceColumn
                && isBackupSupported(index);
        });
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::vms::license::saas;

BackupSettingsViewWidget::BackupSettingsViewWidget(
    SystemContext* system, ServerSettingsDialogStore* store, QWidget* parent):
    base_type(system, backup_settings_view::ColumnCount, parent),
    m_backupSettingsDecoratorModel(new BackupSettingsDecoratorModel(store, systemContext())),
    m_viewItemDelegate(
        new BackupSettingsItemDelegate(resourceViewWidget()->itemViewHoverTracker()))
{
    resourceViewWidget()->itemViewHoverTracker()->setMouseCursorRole(Qn::ItemMouseCursorRole);
    m_viewItemDelegate->setShowRecordingIndicator(true);
    resourceViewWidget()->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resourceViewWidget()->setItemDelegate(m_viewItemDelegate.get());
    resourceViewWidget()->setUniformRowHeights(false);
    resourceViewWidget()->setVisibleItemPredicate(
        [](const QModelIndex& index) { return index.isValid(); }); //< Expand all.

    auto cloudStorageUsageHelper = std::make_shared<CloudStorageServiceUsageHelper>(
        systemContext(), kCloudStorageInfoUpdateInterval, this);

    connect(cloudStorageUsageHelper.get(),
        &CloudStorageServiceUsageHelper::cacheUpdated,
        this,
        &BackupSettingsViewWidget::updateDetailsPanel);

    auto updateFunction = [this, cloudStorageUsageHelper](
        const QModelIndexList& indexes, ResourceDetailsWidget* detailsWidget)
    {
        if (indexes.size() != 1)
        {
            detailsWidget->clear();
            if (indexes.size() == 0 || !m_isCloudBackupStorage)
                return;

            auto toAdd = m_backupSettingsDecoratorModel->backupEnabledCameras();
            for (const auto& index: indexes)
            {
                if (const auto camera = m_backupSettingsDecoratorModel->cameraResource(index); camera &&
                    !toAdd.contains(camera->getId()))
                {
                    toAdd.insert(camera->getId());
                }
            }

            cloudStorageUsageHelper->proposeChange(toAdd, {});
            const auto deficit = cloudStorageUsageHelper->licenseDeficit();
            if (deficit < 1)
                return;

            const auto accentColor = nx::vms::client::core::colorTheme()->color("red");
            detailsWidget->setMessage(
                tr("%1 more suitable cloud storage services are required to activate back up for "
                    "all selected devices", "%1 is the number of cameras for which there are not "
                    "enough licenses in the cloud storage.")
                    .arg(deficit),
                accentColor,
                tr("But you can still use a common switch to enable backups for those cameras for "
                    "which this is possible"));
            detailsWidget->setThumbnailCameraResource({});

            return;
        }

        const auto index = indexes[0];

        if (!index.isValid())
        {
            detailsWidget->clear();
            return;
        }

        const auto resourceIndex = index.siblingAtColumn(backup_settings_view::ResourceColumn);
        detailsWidget->setCaption(backup_settings_view::isNewAddedCamerasRow(index)
            ? QString()
            : resourceIndex.data(Qt::DisplayRole).toString());

        detailsWidget->setThumbnailCameraResource(
            resourceIndex.data(nx::vms::client::core::ResourceRole).value<QnResourcePtr>());

        const auto infoMessageData = index.data(InfoMessageRole);
        if (!infoMessageData.isNull())
        {
            const auto accentColor = nx::vms::client::core::colorTheme()->color("red");
            const auto availableServicesData = index.data(AvailableCloudStorageServices);

            const bool notEnoughServices =
                !availableServicesData.isNull() && availableServicesData.toInt() < 1;
            const bool siteSuspended = systemContext()->saasServiceManager()->saasSuspended();

            if (notEnoughServices || siteSuspended)
                detailsWidget->setMessage(infoMessageData.toString(), accentColor);
            else
                detailsWidget->setMessage(infoMessageData.toString());
        }
        else
        {
            detailsWidget->clearMessage();
        }

        detailsWidget->clearWarningMessages();
        const auto warningMessagesData = index.data(WarningMessagesRole);
        if (!warningMessagesData.isNull())
        {
            const auto warningMessages =
                warningMessagesData.value<QVector<std::pair<QString, QString>>>();
            for (const auto& [caption, message]: warningMessages)
                detailsWidget->addWarningMessage(caption, message);
        }
    };

    setDetailsPanelUpdateFunction(updateFunction);

    // Force default height to be the minimum to avoid unwanted vertical resizes when
    // detailsPanelWidget() shrinks.
    setMinimumHeight(height());

    detailsPanelWidget()->setAutoFillBackground(false);

    m_backupSettingsPicker = new BackupSettingsPickerWidget(store);
    resourceViewWidget()->setFooterWidget(m_backupSettingsPicker);

    connect(resourceViewWidget(), &FilteredResourceViewWidget::selectionChanged, this,
        [this]()
        {
            m_backupSettingsPicker->setupFromSelection(resourceViewWidget()->selectedIndexes());
        });

    connect(m_backupSettingsPicker, &BackupSettingsPickerWidget::backupContentTypesPicked, this,
        [this](BackupContentTypes contentTypes)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());
            m_backupSettingsDecoratorModel->setBackupContentTypes(selectedIndexes, contentTypes);
            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, true);
            m_backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    connect(m_backupSettingsPicker, &BackupSettingsPickerWidget::backupQualityPicked, this,
        [this](CameraBackupQuality quality)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());
            m_backupSettingsDecoratorModel->setBackupQuality(selectedIndexes, quality);
            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, true);
            m_backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    connect(m_backupSettingsPicker, &BackupSettingsPickerWidget::backupEnabledChanged, this,
        [this](bool enabled)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());

            QModelIndexList restoreIndexes;
            std::copy_if(selectedIndexes.cbegin(),
                selectedIndexes.cend(),
                std::back_inserter(restoreIndexes),
                [](const QModelIndex& index)
                {
                    return !index.data(BackupEnabledRole).toBool();
                });

            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, enabled);

            if (enabled)
            {
                QnResourceList cameras;
                std::map<QnResourcePtr, QString> cameraToResolution;

                for (const auto index: selectedIndexes)
                {
                    if (index.data(BackupEnabledRole).toBool())
                        continue;

                    const auto camera = m_backupSettingsDecoratorModel->cameraResource(index);
                    cameras << camera;
                    cameraToResolution[camera] =
                        index.siblingAtColumn(ResolutionColumn).data(Qt::DisplayRole).toString();
                }

                if (!cameras.empty())
                {
                    QnMessageBox messageBox(QnMessageBoxIcon::Warning,
                        tr("Backup will not be activated for %1 devices: ").arg(cameras.size()),
                        tr("%1 suitable cloud storage services are required to activate backup for them.")
                            .arg(cameras.size()),
                        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                        QDialogButtonBox::Ok,
                        this);

                    const auto listView = new QnResourceListView(cameras, &messageBox);
                    const auto delegate =
                        dynamic_cast<QnResourceItemDelegate*>(listView->itemDelegate());
                    delegate->setCustomExtraInfoBuilder(
                        [cameraToResolution](const QnResourcePtr& resource)
                        {
                            const auto value = cameraToResolution.at(resource);
                            return value.isEmpty() ? value : QString(" (%1)").arg(value);
                        });

                    messageBox.addCustomWidget(listView, QnMessageBox::Layout::AfterMainLabel);
                    if (messageBox.exec() == QDialogButtonBox::Cancel)
                        m_backupSettingsDecoratorModel->setBackupEnabled(restoreIndexes, false);
                }
            }
            m_backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    m_backupSettingsPicker->setupLayoutSyncWithHeaderView(resourceViewWidget()->treeHeaderView());

    connect(m_backupSettingsDecoratorModel.get(), &BackupSettingsDecoratorModel::hasChangesChanged,
        this, &BackupSettingsViewWidget::hasChangesChanged);

    connect(m_backupSettingsDecoratorModel.get(),
        &BackupSettingsDecoratorModel::globalBackupSettingsChanged,
        this, &BackupSettingsViewWidget::globalBackupSettingsChanged);

    connect(m_backupSettingsDecoratorModel.get(),
        &BackupSettingsDecoratorModel::globalBackupSettingsChanged,
        this,
        &BackupSettingsViewWidget::updateChangedGlobalSettingsWarning);

    connect(resourceViewWidget(), &FilteredResourceViewWidget::itemClicked, this,
        [this](const QModelIndex& index)
        {
            const auto modifiers = QApplication::keyboardModifiers();
            if (modifiers.testFlag(Qt::ShiftModifier) || modifiers.testFlag(Qt::ControlModifier))
                return;

            switch (index.column())
            {
                case backup_settings_view::SwitchColumn:
                    if (index.data(BackupEnabledRole).isNull())
                        return;

                    switchItemClicked(index);
                    m_backupSettingsPicker->setupFromSelection(
                        resourceViewWidget()->selectedIndexes());
                    break;

                case backup_settings_view::ContentTypesColumn:
                    if (index.data(BackupContentTypesRole).isNull())
                        return;

                    dropdownItemClicked(index);
                    break;

                case backup_settings_view::QualityColumn:
                    if (index.data(BackupQualityRole).isNull())
                        return;

                    dropdownItemClicked(index);
                    break;

                default:
                    break;
            }
        });

    connect(store, &ServerSettingsDialogStore::stateChanged,
        this, &BackupSettingsViewWidget::loadState);
}

BackupSettingsViewWidget::~BackupSettingsViewWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

QAbstractItemModel* BackupSettingsViewWidget::model() const
{
    if (!m_backupSettingsDecoratorModel->sourceModel())
        m_backupSettingsDecoratorModel->setSourceModel(base_type::model());

    return m_backupSettingsDecoratorModel.get();
}

void BackupSettingsViewWidget::setupHeader()
{
    using namespace backup_settings_view;

    static constexpr int kWarningIconColumnWidth = style::Metrics::kDefaultIconSize;
    static constexpr int kSwitchColumnWidth = style::Metrics::kStandardPadding
        + style::Metrics::kButtonSwitchSize.width()
        + style::Metrics::kDefaultTopLevelMargin;

    const auto header = resourceViewWidget()->treeHeaderView();

    header->setVisible(true);
    header->setStretchLastSection(false);
    header->resizeSection(WarningIconColumn, kWarningIconColumnWidth);
    header->resizeSection(SwitchColumn, kSwitchColumnWidth);

    header->setSectionResizeMode(ResourceColumn, QHeaderView::ResizeMode::Stretch);
    header->setSectionResizeMode(ResolutionColumn, QHeaderView::ResizeMode::ResizeToContents);
    header->setSectionResizeMode(WarningIconColumn, QHeaderView::ResizeMode::Fixed);
    header->setSectionResizeMode(ContentTypesColumn, QHeaderView::ResizeMode::ResizeToContents);
    header->setSectionResizeMode(QualityColumn, QHeaderView::ResizeMode::ResizeToContents);
    header->setSectionResizeMode(SwitchColumn, QHeaderView::ResizeMode::Fixed);
    header->setSectionsMovable(false);

    header->setSectionHidden(ResolutionColumn, !m_isCloudBackupStorage);
}

void BackupSettingsViewWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_backupSettingsDecoratorModel->fixBackupContentTypesForCloudStorage();
}

bool BackupSettingsViewWidget::hasChanges() const
{
    return m_backupSettingsDecoratorModel->hasChanges();
}

void BackupSettingsViewWidget::applyChanges()
{
    m_backupSettingsDecoratorModel->applyChanges();
    updateChangedGlobalSettingsWarning();
}

void BackupSettingsViewWidget::discardChanges()
{
    m_backupSettingsDecoratorModel->discardChanges();
    resourceViewWidget()->clearSelection();
    resourceViewWidget()->clearFilter();
    updateChangedGlobalSettingsWarning();

    NX_ASSERT(!isNetworkRequestRunning());
}

bool BackupSettingsViewWidget::isNetworkRequestRunning() const
{
    return m_backupSettingsDecoratorModel->isNetworkRequestRunning();
}

nx::vms::api::BackupSettings BackupSettingsViewWidget::globalBackupSettings() const
{
    return m_backupSettingsDecoratorModel->globalBackupSettings();
}

void BackupSettingsViewWidget::loadState(const ServerSettingsDialogState& state)
{
    m_isCloudBackupStorage = state.backupStoragesStatus.usesCloudBackupStorage;
    resourceViewWidget()->treeHeaderView()->setSectionHidden(
        backup_settings_view::ResolutionColumn, !m_isCloudBackupStorage);

    const auto storageServicesStatus = state.saasProperties.cloudStorageServicesStatus;
    if (storageServicesStatus.status == nx::vms::api::UseStatus::overUse)
    {
        // TODO: #vbreus Following string is poorly translatable. Also figure out which date format should be used.
        const auto bannerMessage = tr("There are more cameras being backed up than available "
            "services. Please disable backup for some cameras or add more suitable services. "
            "Otherwise, it will be done automatically on %1")
                .arg(storageServicesStatus.issueExpirationDate.toString());
        resourceViewWidget()->setInvalidMessage(bannerMessage);
    }
    else
    {
        resourceViewWidget()->setInvalidMessage({});
    }
}

void BackupSettingsViewWidget::switchItemClicked(const QModelIndex& index)
{
    const auto backupEnabledData = index.data(BackupEnabledRole);
    if (!NX_ASSERT(!backupEnabledData.isNull()))
        return;

    m_backupSettingsDecoratorModel->setBackupEnabled({index}, !backupEnabledData.toBool());
}

void BackupSettingsViewWidget::dropdownItemClicked(const QModelIndex& index)
{
    if (!hasDropdownMenu(index))
        return;

    auto menuCreator = std::function<std::unique_ptr<QMenu>(const QModelIndex&)>();
    auto applyFunction = std::function<void(const QModelIndex&, const QVariant&)>();

    if (index.column() == backup_settings_view::ContentTypesColumn)
    {
        menuCreator =
            [this](const auto& index)
            {
                return createContentTypesMenu(
                    m_isCloudBackupStorage,
                    static_cast<BackupContentTypes>(index.data(BackupContentTypesRole).toInt()));
            };

        applyFunction =
            [this](const auto& index, const auto& data)
            {
                if (!backup_settings_view::isNewAddedCamerasRow(index))
                    m_backupSettingsDecoratorModel->setBackupEnabled({index}, true);

                m_backupSettingsDecoratorModel->setBackupContentTypes({index},
                    static_cast<BackupContentTypes>(data.toInt()));
            };
    }

    if (index.column() == backup_settings_view::QualityColumn)
    {
        menuCreator =
            [](const auto& index)
            {
                return createQualityMenu(
                    static_cast<CameraBackupQuality>(index.data(BackupQualityRole).toInt()));
            };

        applyFunction =
            [this](const auto& index, const auto& data)
            {
                if (!backup_settings_view::isNewAddedCamerasRow(index))
                    m_backupSettingsDecoratorModel->setBackupEnabled({index}, true);

                m_backupSettingsDecoratorModel->setBackupQuality({index},
                    static_cast<CameraBackupQuality>(data.toInt()));
            };
    }

    auto menu = menuCreator(index);
    const auto itemRect = resourceViewWidget()->itemRect(index);
    const auto menuPoint = resourceViewWidget()->mapToGlobal(itemRect.bottomLeft());

    QPersistentModelIndex persistentIndex(index);
    connect(menu.get(), &QMenu::triggered,
        [this, applyFunction, persistentIndex](QAction* action)
        {
            if (persistentIndex.isValid())
                applyFunction(persistentIndex, action->data());

            if (const auto picker = qobject_cast<BackupSettingsPickerWidget*>(
                resourceViewWidget()->footerWidget()))
            {
                picker->setupFromSelection(resourceViewWidget()->selectedIndexes());
            }
        });

    connect(menu.get(), &QMenu::aboutToShow, this,
        [this, persistentIndex]
        {
            m_viewItemDelegate->setActiveDropdownIndex(
                resourceViewWidget()->toViewIndex(persistentIndex));
        });

    connect(menu.get(), &QMenu::aboutToHide, this,
        [this] { m_viewItemDelegate->setActiveDropdownIndex({}); });

    connect(menu.get(), &QMenu::aboutToHide, menu.get(), &QMenu::deleteLater);

    menu.release()->popup(menuPoint);
}

void BackupSettingsViewWidget::updateChangedGlobalSettingsWarning()
{
    auto modelBackupNewCameras =
        m_backupSettingsDecoratorModel->globalBackupSettings().backupNewCameras;
    auto savedBackupNewCameras = systemSettings()->backupSettings().backupNewCameras;
    resourceViewWidget()->setInfoMessage(savedBackupNewCameras && !modelBackupNewCameras
        ? tr("Backup will be turned off for newly added cameras on all servers at the site")
        : QString());
}

} // namespace nx::vms::client::desktop
