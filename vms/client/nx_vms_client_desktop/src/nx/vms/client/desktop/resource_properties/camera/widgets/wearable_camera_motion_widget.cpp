#include "wearable_camera_motion_widget.h"
#include "ui_wearable_camera_motion_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/core/motion/motion_grid.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

WearableCameraMotionWidget::WearableCameraMotionWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::WearableCameraMotionWidget),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);
    m_aligner->addWidget(ui->sensitivityLabel);
    check_box_utils::autoClearTristate(ui->motionDetectionCheckBox);

    combo_box_utils::insertMultipleValuesItem(ui->sensitivityComboBox);

    for (int i = 1; i < QnMotionRegion::kSensitivityLevelCount; i++)
        ui->sensitivityComboBox->addItem(QString::number(i));
}

WearableCameraMotionWidget::~WearableCameraMotionWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

Aligner* WearableCameraMotionWidget::aligner() const
{
    return m_aligner;
}

void WearableCameraMotionWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &WearableCameraMotionWidget::loadState);

    m_storeConnections << connect(ui->motionDetectionCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setWearableMotionDetectionEnabled);

    m_storeConnections << connect(ui->sensitivityComboBox, QnComboboxCurrentIndexChanged,
        store, &CameraSettingsDialogStore::setWearableMotionSensitivity);
}

void WearableCameraMotionWidget::loadState(const CameraSettingsDialogState& state)
{
    setReadOnly(ui->motionDetectionCheckBox, state.readOnly);
    setReadOnly(ui->sensitivityComboBox, state.readOnly);

    check_box_utils::setupTristateCheckbox(ui->motionDetectionCheckBox, state.wearableMotion.enabled);

    ui->sensitivityWidget->setVisible(ui->motionDetectionCheckBox->checkState() == Qt::Checked);
    ui->sensitivityComboBox->setCurrentIndex(state.wearableMotion.sensitivity.valueOr(0));

    ui->groupBox->layout()->activate();
    layout()->activate();
}

} // namespace nx::vms::client::desktop
