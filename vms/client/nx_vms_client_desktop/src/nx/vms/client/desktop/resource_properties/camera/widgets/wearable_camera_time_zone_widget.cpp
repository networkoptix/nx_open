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

    m_storeConnections << connect(ui->clientRadioButton, &QRadioButton::toggled,
        store, &CameraSettingsDialogStore::setWearableClientTimeZone);
}

void WearableCameraTimeZoneWidget::loadState(const CameraSettingsDialogState& state)
{
    setReadOnly(ui->autoRadioButton, state.readOnly);
    setReadOnly(ui->clientRadioButton, state.readOnly);

    if (state.wearableClientTimeZone)
        ui->clientRadioButton->toggle();
    else
        ui->autoRadioButton->toggle();
}

} // namespace nx::vms::client::desktop
