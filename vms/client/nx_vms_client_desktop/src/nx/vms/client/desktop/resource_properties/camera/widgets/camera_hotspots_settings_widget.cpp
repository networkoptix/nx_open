// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_settings_widget.h"
#include "ui_camera_hotspots_settings_widget.h"

#include <memory>

#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/utils/camera_hotspots_support.h>
#include <ui/common/palette.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace {

static const auto kMaximumHotspotEditorHeight = 480;

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget::Private declaration.

struct CameraHotspotsSettingsWidget::Private
{
    CameraHotspotsSettingsWidget* const q;

    const std::unique_ptr<Ui::CameraHotspotsSettingsWidget> ui =
        std::make_unique<Ui::CameraHotspotsSettingsWidget>();

    void setupUi() const;

    void updateHotspotEditorPanelVisibility() const;
    void updateHotspotCameraSelectionButton() const;

    const std::unique_ptr<CameraSelectionDialog> createHotspotCameraSelectionDialog(
        int hotspotIndex) const;

    CameraSelectionDialog::ResourceFilter hotspotCameraSelectionResourceFilter(
        int hotspotIndex) const;

    void selectCameraForHotspot(int index);

    QnUuid singleCameraId;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget::Private definition.

void CameraHotspotsSettingsWidget::Private::setupUi() const
{
    ui->setupUi(q);

    ui->widgetLayout->setContentsMargins(nx::style::Metrics::kDefaultTopLevelMargins);

    auto hotspotEditControlsLayoutMargins = nx::style::Metrics::kDefaultTopLevelMargins;
    hotspotEditControlsLayoutMargins.setBottom(
        hotspotEditControlsLayoutMargins.bottom() - ui->hotspotsEditorBottomLine->lineWidth());
    ui->hotspotEditControlsLayout->setContentsMargins(hotspotEditControlsLayoutMargins);

    setPaletteColor(ui->hotspotEditControlsPanel, QPalette::Window, colorTheme()->color("dark8"));
    ui->hotspotEditControlsPanel->setAutoFillBackground(true);

    ui->addHotspotButton->setIcon(qnSkin->icon("buttons/plus.png"));
    ui->deleteHotspotButton->setIcon(qnSkin->icon("text_buttons/trash.png"));

    ui->hotspotsEditorWidget->setMaximumHeight(kMaximumHotspotEditorHeight);

    updateHotspotEditorPanelVisibility();
}

void CameraHotspotsSettingsWidget::Private::updateHotspotEditorPanelVisibility() const
{
    const auto panelVisible = ui->hotspotsEditorWidget->selectedHotspotIndex().has_value();
    ui->hotspotEditControlsPanel->setHidden(!panelVisible);
    ui->hotspotsEditorBottomLine->setHidden(!panelVisible);
}

void CameraHotspotsSettingsWidget::Private::updateHotspotCameraSelectionButton() const
{
    const auto camerasIcon = qnResIconCache->icon(QnResourceIconCache::Cameras);
    const auto cameraIcon = qnResIconCache->icon(QnResourceIconCache::Camera);
    const auto invalidCameraIcon =
        qnSkin->colorize(qnSkin->maximumSizePixmap(cameraIcon), colorTheme()->color("red_l2"));

    if (const auto selectedIndex = ui->hotspotsEditorWidget->selectedHotspotIndex())
    {
        const auto cameraId = ui->hotspotsEditorWidget->hotspotAt(selectedIndex.value()).cameraId;
        if (!cameraId.isNull())
        {
            const auto camera = appContext()->currentSystemContext()->resourcePool()
                ->getResourceById<QnVirtualCameraResource>(cameraId);

            if (!camera.isNull())
            {
                resetStyle(ui->selectCameraButton);
                ui->selectCameraButton->setIcon(cameraIcon);
                ui->selectCameraButton->setText(camera->getUserDefinedName());
            }
            else
            {
                setWarningStyle(ui->selectCameraButton);
                ui->selectCameraButton->setIcon(invalidCameraIcon);
                ui->selectCameraButton->setText(tr("Camera doesn't exist"));
            }
            return;
        }
    }

    resetStyle(ui->selectCameraButton);
    ui->selectCameraButton->setIcon(camerasIcon);
    ui->selectCameraButton->setText(tr("Select Camera"));
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

    if (const auto id = ui->hotspotsEditorWidget->hotspotAt(hotspotIndex).cameraId; !id.isNull())
        resourceSelectionWidget->setSelectedResourceId(id);

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

        if (const auto id = ui->hotspotsEditorWidget->hotspotAt(i).cameraId; !id.isNull())
            usedHotspotCamerasIds.insert(id);
    }

    return
        [usedHotspotCamerasIds, this](const QnResourcePtr& resource)
        {
            return nx::vms::common::camera_hotspots::hotspotCanReferToCamera(resource)
                && !usedHotspotCamerasIds.contains(resource->getId())
                && resource->getId() != singleCameraId;
        };
}

void CameraHotspotsSettingsWidget::Private::selectCameraForHotspot(int hotspotIndex)
{
    const auto cameraSelectionDialog = createHotspotCameraSelectionDialog(hotspotIndex);
    if (cameraSelectionDialog->exec() != QDialog::Accepted)
        return;

    auto hotspotData = ui->hotspotsEditorWidget->hotspotAt(hotspotIndex);
    hotspotData.cameraId =
        cameraSelectionDialog->resourceSelectionWidget()->selectedResourceId();

    ui->hotspotsEditorWidget->setHotspotAt(hotspotData, hotspotIndex);
    updateHotspotCameraSelectionButton();
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotsSettingsWidget definition.

CameraHotspotsSettingsWidget::CameraHotspotsSettingsWidget(
    CameraSettingsDialogStore* store,
    const QSharedPointer<LiveCameraThumbnail>& thumbnail,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private{this})
{
    d->setupUi();
    d->ui->hotspotsEditorWidget->setThumbnail(thumbnail);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        [this](const CameraSettingsDialogState& state)
        {
            d->ui->hotspotsEditorWidget->setHotspots(state.isSingleCamera()
                ? state.singleCameraSettings.cameraHotspots
                : nx::vms::common::CameraHotspotDataList());
            d->singleCameraId = state.singleCameraId();
        });

    connect(d->ui->addHotspotButton, &QPushButton::clicked, this,
        [this]
        {
            auto index = d->ui->hotspotsEditorWidget->appendHotspot({{}, {0.5, 0.5}});
            d->ui->hotspotsEditorWidget->setSelectedHotspotIndex(index);
        });

    connect(d->ui->deleteHotspotButton, &QPushButton::clicked, this,
        [this]
        {
            if (const auto selectedIndex = d->ui->hotspotsEditorWidget->selectedHotspotIndex())
                d->ui->hotspotsEditorWidget->removeHotstpotAt(selectedIndex.value());
        });

    connect(d->ui->selectCameraButton, &QPushButton::clicked, this,
        [this]
        {
            if (!NX_ASSERT(d->ui->hotspotsEditorWidget->selectedHotspotIndex().has_value()))
                return;

            const auto selectedIndex = d->ui->hotspotsEditorWidget->selectedHotspotIndex().value();
            d->selectCameraForHotspot(selectedIndex);
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::selectedHotspotChanged, this,
        [this]
        {
            d->updateHotspotCameraSelectionButton();
            d->updateHotspotEditorPanelVisibility();
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::hotspotsDataChanged, this,
        [this, store]
        {
            store->setCameraHotspotsData(d->ui->hotspotsEditorWidget->getHotspots());
        });

    connect(d->ui->hotspotsEditorWidget, &CameraHotspotsEditorWidget::selectHotspotCamera, this,
        [this](int index)
        {
            d->ui->hotspotsEditorWidget->setSelectedHotspotIndex(index);
            d->selectCameraForHotspot(index);
        });
}

CameraHotspotsSettingsWidget::~CameraHotspotsSettingsWidget()
{
}

} // namespace nx::vms::client::desktop
