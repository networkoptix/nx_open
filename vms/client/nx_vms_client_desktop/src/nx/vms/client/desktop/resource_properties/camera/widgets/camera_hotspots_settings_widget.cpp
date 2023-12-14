// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_settings_widget.h"
#include "ui_camera_hotspots_settings_widget.h"

#include <memory>

#include <QtCore/QItemSelectionModel>

#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/camera_hotspots_item_delegate.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/camera_hotspots_item_model.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/utils/camera_hotspots_support.h>
#include <ui/common/palette.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace {

static const auto kMaximumHotspotEditorHeight = 448;
static constexpr auto kMinimumTableEditorHeight = 112;

static const QColor kBasicColor = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QnIcon::Normal, {{kBasicColor, "light4"}}},
    {QnIcon::Active, {{kBasicColor, "light3"}}},
    {QnIcon::Selected, {{kBasicColor, "light5"}}},
};

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget::Private declaration.

struct CameraHotspotsSettingsWidget::Private
{
    Private(CameraHotspotsSettingsWidget* q, QnResourcePool* resourcePool):
        q(q),
        ui(new Ui::CameraHotspotsSettingsWidget),
        hotspotsModel(new CameraHotspotsItemModel(resourcePool)),
        hotspotsDelegate(new CameraHotspotsItemDelegate)
    {
    }

    CameraHotspotsSettingsWidget* const q;
    const std::unique_ptr<Ui::CameraHotspotsSettingsWidget> ui;
    const std::unique_ptr<CameraHotspotsItemModel> hotspotsModel;
    const std::unique_ptr<CameraHotspotsItemDelegate> hotspotsDelegate;

    QnUuid cameraId;

    void setupUi() const;

    const std::unique_ptr<CameraSelectionDialog> createHotspotCameraSelectionDialog(
        int hotspotIndex) const;

    CameraSelectionDialog::ResourceFilter hotspotCameraSelectionResourceFilter(
        int hotspotIndex) const;

    void selectCameraForHotspot(int index) const;

    void selectHotspotsViewRow(std::optional<int> row);
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget::Private definition.

void CameraHotspotsSettingsWidget::Private::setupUi() const
{
    ui->setupUi(q);

    auto contentMargins(nx::style::Metrics::kDefaultTopLevelMargins);
    contentMargins.setBottom(0);
    ui->widgetLayout->setContentsMargins(contentMargins);

    ui->enableHotspotsCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->enableHotspotsCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->addHotspotButton->setIcon(qnSkin->icon("buttons/add_20x20_deprecated.svg", kIconSubstitutions));

    ui->hotspotsEditorWidget->setMaximumHeight(kMaximumHotspotEditorHeight);

    const auto hotspotsItemViewhoverTracker = new ItemViewHoverTracker(ui->hotspotsItemView);
    hotspotsItemViewhoverTracker->setMouseCursorRole(Qn::ItemMouseCursorRole);
    hotspotsDelegate->setItemViewHoverTracker(hotspotsItemViewhoverTracker);

    hotspotsItemViewhoverTracker->setHoverMaskPredicate(
        [this](const QModelIndex& index, const QPoint& viewportPos)
        {
            if (index.column() != CameraHotspotsItemModel::CameraColumn)
                return true;

            return hotspotsDelegate->itemContentsRect(
                index, ui->hotspotsItemView).contains(viewportPos);
        });

    ui->hotspotsItemView->setMinimumHeight(kMinimumTableEditorHeight);
    ui->hotspotsItemView->setItemDelegate(hotspotsDelegate.get());
    ui->hotspotsItemView->setModel(hotspotsModel.get());
    ui->hotspotsItemView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto header = ui->hotspotsItemView->header();
    header->setSectionResizeMode(
        CameraHotspotsItemModel::IndexColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        CameraHotspotsItemModel::CameraColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        CameraHotspotsItemModel::ColorPaletteColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        CameraHotspotsItemModel::PointedCheckBoxColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        CameraHotspotsItemModel::DeleteButtonColumn, QHeaderView::ResizeToContents);
}

const std::unique_ptr<CameraSelectionDialog>
    CameraHotspotsSettingsWidget::Private::createHotspotCameraSelectionDialog(
        int hotspotIndex) const
{
    auto cameraSelectionDialog = std::make_unique<CameraSelectionDialog>(
        hotspotCameraSelectionResourceFilter(hotspotIndex),
        CameraSelectionDialog::ResourceValidator(),
        CameraSelectionDialog::AlertTextProvider(),
        q);

    cameraSelectionDialog->setAllCamerasSwitchVisible(false);
    cameraSelectionDialog->setAllowInvalidSelection(false);

    const auto resourceSelectionWidget = cameraSelectionDialog->resourceSelectionWidget();
    resourceSelectionWidget->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);
    resourceSelectionWidget->setShowRecordingIndicator(true);

    if (const auto id = ui->hotspotsEditorWidget->hotspotAt(hotspotIndex).targetResourceId;
        !id.isNull())
    {
        resourceSelectionWidget->setSelectedResourceId(id);
    }

    return cameraSelectionDialog;
}

CameraSelectionDialog::ResourceFilter
    CameraHotspotsSettingsWidget::Private::hotspotCameraSelectionResourceFilter(
        int hotspotIndex) const
{
    QnUuidSet usedHotspotCamerasIds;
    for (int i = 0; i < ui->hotspotsEditorWidget->hotspotsCount(); ++i)
    {
        if (i == hotspotIndex)
            continue;

        if (const auto id = ui->hotspotsEditorWidget->hotspotAt(i).targetResourceId; !id.isNull())
            usedHotspotCamerasIds.insert(id);
    }

    return
        [usedHotspotCamerasIds, this](const QnResourcePtr& resource)
        {
            return nx::vms::common::camera_hotspots::hotspotCanReferToCamera(resource)
                && !usedHotspotCamerasIds.contains(resource->getId())
                && resource->getId() != cameraId;
        };
}

void CameraHotspotsSettingsWidget::Private::selectCameraForHotspot(int hotspotIndex) const
{
    const auto cameraSelectionDialog = createHotspotCameraSelectionDialog(hotspotIndex);
    if (cameraSelectionDialog->exec() != QDialog::Accepted)
        return;

    auto hotspotData = ui->hotspotsEditorWidget->hotspotAt(hotspotIndex);
    hotspotData.targetResourceId =
        cameraSelectionDialog->resourceSelectionWidget()->selectedResourceId();

    ui->hotspotsEditorWidget->setHotspotAt(hotspotData, hotspotIndex);
}

void CameraHotspotsSettingsWidget::Private::selectHotspotsViewRow(std::optional<int> row)
{
    if (row)
    {
        const auto selectionModel = ui->hotspotsItemView->selectionModel();
        const auto index = hotspotsModel->index(*row, 0);
        ui->hotspotsItemView->scrollTo(index);
        selectionModel->select(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    else
    {
        ui->hotspotsItemView->clearSelection();
    }
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget definition.

CameraHotspotsSettingsWidget::CameraHotspotsSettingsWidget(
    QnResourcePool* resourcePool,
    CameraSettingsDialogStore* store,
    const QSharedPointer<LiveCameraThumbnail>& thumbnail,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, resourcePool))
{
    d->setupUi();
    d->ui->hotspotsEditorWidget->setThumbnail(thumbnail);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        [this](const CameraSettingsDialogState& state)
        {
            const bool hotspotsEnabled = state.singleCameraSettings.cameraHotspotsEnabled();

            d->ui->enableHotspotsCheckBox->setChecked(hotspotsEnabled);
            d->ui->hotspotsEditorWidget->setHotspots(state.isSingleCamera()
                ? state.singleCameraSettings.cameraHotspots.get()
                : nx::vms::common::CameraHotspotDataList());

            d->hotspotsModel->setHotspots(state.isSingleCamera()
                ? state.singleCameraSettings.cameraHotspots.get()
                : nx::vms::common::CameraHotspotDataList());

            d->cameraId = state.singleCameraId();
            d->ui->hotspotsEditorWidget->setEnabled(hotspotsEnabled);
            d->ui->hotspotsItemView->setEnabled(hotspotsEnabled);
            d->ui->addHotspotButton->setEnabled(hotspotsEnabled);
            if (!hotspotsEnabled)
            {
                d->ui->hotspotsEditorWidget->setSelectedHotspotIndex({});
                d->ui->hotspotsItemView->scrollToTop();
            }
        });

    connect(d->ui->addHotspotButton, &QPushButton::clicked, this,
        [this]
        {
            static const auto kHotspotsPalette = CameraHotspotsItemDelegate::hotspotsPalette();

            nx::vms::common::CameraHotspotData hotspot;
            hotspot.pos = {0.5, 0.5};
            if (NX_ASSERT(!kHotspotsPalette.empty()))
                hotspot.accentColorName = kHotspotsPalette.first().name();
            auto index = d->ui->hotspotsEditorWidget->appendHotspot(hotspot);
            d->ui->hotspotsEditorWidget->setSelectedHotspotIndex(index);
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::selectedHotspotChanged, this,
        [this]
        {
            d->selectHotspotsViewRow(d->ui->hotspotsEditorWidget->selectedHotspotIndex());
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::hotspotsDataChanged, this,
        [this, store]
        {
            store->setCameraHotspotsData(d->ui->hotspotsEditorWidget->getHotspots());
            d->selectHotspotsViewRow(d->ui->hotspotsEditorWidget->selectedHotspotIndex());
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::selectHotspotCamera, this,
        [this](int index)
        {
            d->ui->hotspotsEditorWidget->setSelectedHotspotIndex(index);
            d->selectCameraForHotspot(index);
        });

    connect(d->ui->enableHotspotsCheckBox, &QCheckBox::clicked, store,
        &CameraSettingsDialogStore::setCameraHotspotsEnabled);

    connect(d->ui->hotspotsItemView, &QTreeView::clicked, this,
        [this](const QModelIndex& index)
        {
            const auto hotspotsView = d->ui->hotspotsItemView;
            const auto hotspotsEditor = d->ui->hotspotsEditorWidget;
            const auto hotspotIndex = index.row();
            auto hotspot = hotspotsEditor->hotspotAt(hotspotIndex);

            if (index.column() == CameraHotspotsItemModel::CameraColumn)
            {
                const auto viewportCursorPos =
                    hotspotsView->viewport()->mapFromGlobal(QCursor::pos());

                if (d->hotspotsDelegate->itemContentsRect(index, hotspotsView)
                    .contains(viewportCursorPos))
                {
                    d->selectCameraForHotspot(index.row());
                }
            }

            if (index.column() == CameraHotspotsItemModel::ColorPaletteColumn)
            {
                const auto viewportCursorPos =
                    hotspotsView->viewport()->mapFromGlobal(QCursor::pos());

                if (const auto colorIndex = d->hotspotsDelegate->paletteColorIndexAtPos(
                    viewportCursorPos, hotspotsView->visualRect(index)))
                {
                    hotspot.accentColorName =
                        CameraHotspotsItemDelegate::hotspotsPalette().at(*colorIndex).name();

                    hotspotsEditor->setHotspotAt(hotspot, hotspotIndex);
                    d->selectHotspotsViewRow(hotspotIndex);
                }
            }

            if (index.column() == CameraHotspotsItemModel::DeleteButtonColumn)
                hotspotsEditor->removeHotstpotAt(hotspotIndex);

            if (index.column() == CameraHotspotsItemModel::PointedCheckBoxColumn)
            {
                if (hotspot.hasDirection())
                {
                    hotspot.direction = QPointF();
                }
                else
                {
                    using nx::vms::client::core::Geometry;
                    hotspot.direction = qFuzzyEquals(hotspot.pos, QPointF(0.5, 0.5))
                        ? QPointF(0.0, -1.0)
                        : Geometry::normalized(hotspot.pos - QPointF(0.5, 0.5));
                }

                hotspotsEditor->setHotspotAt(hotspot, hotspotIndex);
                d->selectHotspotsViewRow(hotspotIndex);
            }
        });

    connect(d->ui->hotspotsItemView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
        [this]
        {
            const auto selection = d->ui->hotspotsItemView->selectionModel()->selection();
            d->ui->hotspotsEditorWidget->setSelectedHotspotIndex(!selection.empty()
                ? std::optional<int>(selection.indexes().first().row())
                : std::nullopt);
        });
}

CameraHotspotsSettingsWidget::~CameraHotspotsSettingsWidget()
{
}

} // namespace nx::vms::client::desktop
