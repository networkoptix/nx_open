#include "wearable_camera_time_zone_widget.h"
#include "ui_wearable_camera_time_zone_widget.h"

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

WearableCameraTimeZoneWidget::WearableCameraTimeZoneWidget(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::WearableCameraTimeZoneWidget)
{
    ui->setupUi(this);
}

WearableCameraTimeZoneWidget::~WearableCameraTimeZoneWidget()
{
}

void WearableCameraTimeZoneWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &WearableCameraTimeZoneWidget::loadState);

    m_storeConnections << connect(ui->ignoreTimeZoneCheckBox, &QCheckBox::toggled,
        store, &CameraSettingsDialogStore::setWearableIgnoreTimeZone);
}

void WearableCameraTimeZoneWidget::loadState(const CameraSettingsDialogState& state)
{
    setReadOnly(ui->ignoreTimeZoneCheckBox, state.readOnly);

    ui->ignoreTimeZoneCheckBox->setChecked(state.wearableIgnoreTimeZone);
}

} // namespace nx::vms::client::desktop
