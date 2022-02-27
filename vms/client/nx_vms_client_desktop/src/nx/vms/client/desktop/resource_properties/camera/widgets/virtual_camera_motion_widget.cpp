// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_motion_widget.h"
#include "ui_virtual_camera_motion_widget.h"

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/motion/motion_grid.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

VirtualCameraMotionWidget::VirtualCameraMotionWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::VirtualCameraMotionWidget),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);
    m_aligner->addWidget(ui->sensitivityLabel);
    check_box_utils::autoClearTristate(ui->motionDetectionCheckBox);

    combo_box_utils::insertMultipleValuesItem(ui->sensitivityComboBox);

    for (int i = 1; i < QnMotionRegion::kSensitivityLevelCount; i++)
        ui->sensitivityComboBox->addItem(QString::number(i));

    ui->motionHintLabel->setText(tr("Motion is being detected only during video uploading.")
        + '\n'
        + tr("Enabling or disabling this setting does not change anything in the existing archive."));
}

VirtualCameraMotionWidget::~VirtualCameraMotionWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

Aligner* VirtualCameraMotionWidget::aligner() const
{
    return m_aligner;
}

void VirtualCameraMotionWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &VirtualCameraMotionWidget::loadState);

    m_storeConnections << connect(ui->motionDetectionCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setVirtualCameraMotionDetectionEnabled);

    m_storeConnections << connect(ui->sensitivityComboBox, QnComboboxCurrentIndexChanged,
        store, &CameraSettingsDialogStore::setVirtualCameraMotionSensitivity);
}

void VirtualCameraMotionWidget::loadState(const CameraSettingsDialogState& state)
{
    setReadOnly(ui->motionDetectionCheckBox, state.readOnly);
    setReadOnly(ui->sensitivityComboBox, state.readOnly);

    check_box_utils::setupTristateCheckbox(ui->motionDetectionCheckBox, state.virtualCameraMotion.enabled);

    ui->sensitivityWidget->setVisible(ui->motionDetectionCheckBox->checkState() == Qt::Checked);
    ui->sensitivityComboBox->setCurrentIndex(state.virtualCameraMotion.sensitivity.valueOr(0));

    ui->groupBox->layout()->activate();
    layout()->activate();
}

} // namespace nx::vms::client::desktop
