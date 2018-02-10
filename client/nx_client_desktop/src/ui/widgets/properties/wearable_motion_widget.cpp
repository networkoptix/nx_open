#include "wearable_motion_widget.h"
#include "ui_wearable_motion_widget.h"

#include <core/resource/camera_resource.h>

#include <ui/common/aligner.h>

QnWearableMotionWidget::QnWearableMotionWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::WearableMotionWidget)
{
    ui->setupUi(this);

    m_aligner = new QnAligner(this);
    m_aligner->addWidget(ui->sensitivityLabel);

    connect(ui->motionDetectionCheckBox, &QCheckBox::stateChanged, this,
        &QnWearableMotionWidget::updateSensitivityEnabled);

    updateSensitivityEnabled();
}

QnWearableMotionWidget::~QnWearableMotionWidget()
{
}

QnAligner* QnWearableMotionWidget::aligner() const
{
    return m_aligner;
}

void QnWearableMotionWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    //updateFromState(qnClientModule->wearableManager()->state(camera));
}

QnVirtualCameraResourcePtr QnWearableMotionWidget::camera() const
{
    return m_camera;
}

void QnWearableMotionWidget::updateSensitivityEnabled()
{
    bool enabled = ui->motionDetectionCheckBox->isChecked();

    ui->sensitivityLabel->setEnabled(enabled);
    ui->sensitivitySpinBox->setEnabled(enabled);
}
