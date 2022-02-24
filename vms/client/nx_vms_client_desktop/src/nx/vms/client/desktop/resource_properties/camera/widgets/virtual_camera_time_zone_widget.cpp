// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_time_zone_widget.h"
#include "ui_virtual_camera_time_zone_widget.h"

#include <nx/utils/scoped_connections.h>
#include <ui/common/read_only.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

VirtualCameraTimeZoneWidget::VirtualCameraTimeZoneWidget(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::VirtualCameraTimeZoneWidget)
{
    ui->setupUi(this);
}

VirtualCameraTimeZoneWidget::~VirtualCameraTimeZoneWidget()
{
}

void VirtualCameraTimeZoneWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &VirtualCameraTimeZoneWidget::loadState);

    m_storeConnections << connect(ui->ignoreTimeZoneCheckBox, &QCheckBox::toggled,
        store, &CameraSettingsDialogStore::setVirtualCameraIgnoreTimeZone);
}

void VirtualCameraTimeZoneWidget::loadState(const CameraSettingsDialogState& state)
{
    setReadOnly(ui->ignoreTimeZoneCheckBox, state.readOnly);

    ui->ignoreTimeZoneCheckBox->setChecked(state.virtualCameraIgnoreTimeZone);
}

} // namespace nx::vms::client::desktop
