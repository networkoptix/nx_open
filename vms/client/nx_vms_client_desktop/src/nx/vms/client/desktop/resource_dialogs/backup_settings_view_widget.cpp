// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_view_widget.h"

#include <QtCore/QPersistentModelIndex>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#include <core/resource/camera_resource.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/details/resource_details_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/backup_settings_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/backup_settings_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/backup_settings_picker_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace backup_settings_view;

using AbstractEntityPtr = entity_item_model::AbstractEntityPtr;
using ResourceTreeEntityBuilder = entity_resource_tree::ResourceTreeEntityBuilder;

void detailsPanelUpdateFunction(const QModelIndex& index, ResourceDetailsWidget* detailsWidget)
{
    if (!index.isValid())
    {
        detailsWidget->clear();
        return;
    }

    detailsWidget->setUpdatesEnabled(false);

    const auto resourceIndex = index.siblingAtColumn(backup_settings_view::ResourceColumn);
    detailsWidget->setCaptionText(backup_settings_view::isNewAddedCamerasRow(index)
        ? QString()
        : resourceIndex.data(Qt::DisplayRole).toString());

    detailsWidget->setThumbnailCameraResource(
        resourceIndex.data(Qn::ResourceRole).value<QnResourcePtr>());

    const auto nothingToBackupWarningData =
        index.siblingAtColumn(backup_settings_view::ContentTypesColumn)
            .data(NothingToBackupWarningRole);

    detailsWidget->setWarningCaptionText(nothingToBackupWarningData.isNull()
        ? QString()
        : BackupSettingsViewWidget::tr("Nothing to backup"));
    detailsWidget->setWarningExplanationText(nothingToBackupWarningData.toString());

    detailsWidget->setUpdatesEnabled(true);
}

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

BackupSettingsViewWidget::BackupSettingsViewWidget(QWidget* parent):
    base_type(backup_settings_view::ColumnCount, parent),
    m_backupSettingsDecoratorModel(new BackupSettingsDecoratorModel(systemContext())),
    m_viewItemDelegate(new BackupSettingsItemDelegate(resourceViewWidget()->itemViewHoverTracker()))
{
    m_viewItemDelegate->setShowRecordingIndicator(true);
    resourceViewWidget()->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resourceViewWidget()->setItemDelegate(m_viewItemDelegate.get());
    resourceViewWidget()->setUniformRowHeights(false);
    resourceViewWidget()->setVisibleItemPredicate(
        [](const QModelIndex& index) { return index.isValid(); }); //< Expand all.

    setDetailsPanelUpdateFunction(detailsPanelUpdateFunction);

    // Force default height to be the minimum to avoid unwanted vertical resizes when
    // detailsPanelWidget() shrinks.
    setMinimumHeight(height());

    detailsPanelWidget()->setAutoFillBackground(false);

    auto backupSettingsPicker = new BackupSettingsPickerWidget();
    resourceViewWidget()->setFooterWidget(backupSettingsPicker);

    connect(resourceViewWidget(), &FilteredResourceViewWidget::selectionChanged, this,
        [this, backupSettingsPicker]()
        {
            backupSettingsPicker->setupFromSelection(resourceViewWidget()->selectedIndexes());
        });

    connect(backupSettingsPicker, &BackupSettingsPickerWidget::backupContentTypesPicked, this,
        [this, backupSettingsPicker](BackupContentTypes contentTypes)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());
            m_backupSettingsDecoratorModel->setBackupContentTypes(selectedIndexes, contentTypes);
            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, true);
            backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    connect(backupSettingsPicker, &BackupSettingsPickerWidget::backupQualityPicked, this,
        [this, backupSettingsPicker](CameraBackupQuality quality)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());
            m_backupSettingsDecoratorModel->setBackupQuality(selectedIndexes, quality);
            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, true);
            backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    connect(backupSettingsPicker, &BackupSettingsPickerWidget::backupEnabledChanged, this,
        [this, backupSettingsPicker](bool enabled)
        {
            const auto selectedIndexes =
                filterSelectedIndexes(resourceViewWidget()->selectedIndexes());
            m_backupSettingsDecoratorModel->setBackupEnabled(selectedIndexes, enabled);
            backupSettingsPicker->setupFromSelection(selectedIndexes);
        });

    backupSettingsPicker->setupLayoutSyncWithHeaderView(resourceViewWidget()->treeHeaderView());

    connect(m_backupSettingsDecoratorModel.get(), &BackupSettingsDecoratorModel::hasChangesChanged,
        this, &BackupSettingsViewWidget::hasChangesChanged);

    connect(m_backupSettingsDecoratorModel.get(),
        &BackupSettingsDecoratorModel::globalBackupSettingsChanged,
        this, &BackupSettingsViewWidget::globalBackupSettingsChanged);

    connect(resourceViewWidget(), &FilteredResourceViewWidget::itemClicked, this,
        [this, backupSettingsPicker](const QModelIndex& index)
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
                    backupSettingsPicker->setupFromSelection(
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
}

BackupSettingsViewWidget::~BackupSettingsViewWidget()
{
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
    header->setSectionResizeMode(WarningIconColumn, QHeaderView::ResizeMode::Fixed);
    header->setSectionResizeMode(ContentTypesColumn, QHeaderView::ResizeMode::ResizeToContents);
    header->setSectionResizeMode(QualityColumn, QHeaderView::ResizeMode::ResizeToContents);
    header->setSectionResizeMode(SwitchColumn, QHeaderView::ResizeMode::Fixed);
    header->setSectionsMovable(false);
}

bool BackupSettingsViewWidget::hasChanges() const
{
    return m_backupSettingsDecoratorModel->hasChanges();
}

void BackupSettingsViewWidget::loadDataToUi()
{
}

void BackupSettingsViewWidget::applyChanges()
{
    m_backupSettingsDecoratorModel->applyChanges();
}

void BackupSettingsViewWidget::discardChanges()
{
    m_backupSettingsDecoratorModel->discardChanges();
    resourceViewWidget()->clearSelection();
    resourceViewWidget()->clearFilter();
}

nx::vms::api::BackupSettings BackupSettingsViewWidget::globalBackupSettings() const
{
    return m_backupSettingsDecoratorModel->globalBackupSettings();
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
            [](const auto& index)
            {
                return createContentTypesMenu(
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

} // namespace nx::vms::client::desktop
