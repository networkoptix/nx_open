#include "wearable_motion_widget.h"
#include "ui_wearable_motion_widget.h"

#include <core/resource/camera_resource.h>

#include <ui/common/aligner.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

QnWearableMotionWidget::QnWearableMotionWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::WearableMotionWidget)
{
    ui->setupUi(this);

    ui->sensitivitySpinBox->setMinimum(1);
    ui->sensitivitySpinBox->setMaximum(QnMotionRegion::kSensitivityLevelCount - 1);

    m_aligner = new QnAligner(this);
    m_aligner->addWidget(ui->sensitivityLabel);

    connect(ui->motionDetectionCheckBox, &QCheckBox::stateChanged, this,
        &QnWearableMotionWidget::changed);
    connect(ui->sensitivitySpinBox, QnSpinboxIntValueChanged, this,
        &QnWearableMotionWidget::changed);

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

bool QnWearableMotionWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnWearableMotionWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->motionDetectionCheckBox, readOnly);
    setReadOnly(ui->sensitivitySpinBox, readOnly);

    m_readOnly = readOnly;
}

void QnWearableMotionWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    ui->motionDetectionCheckBox->setChecked(camera->getMotionType() == Qn::MT_SoftwareGrid);
    ui->sensitivitySpinBox->setValue(calculateSensitivity(camera));
}

void QnWearableMotionWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    camera->setMotionType(ui->motionDetectionCheckBox->isChecked() ? Qn::MT_SoftwareGrid : Qn::MT_NoMotion);
    submitSensitivity(camera, ui->sensitivitySpinBox->value());
}

void QnWearableMotionWidget::updateSensitivityEnabled()
{
    bool enabled = ui->motionDetectionCheckBox->isChecked();

    ui->sensitivityLabel->setEnabled(enabled);
    ui->sensitivitySpinBox->setEnabled(enabled);
}

int QnWearableMotionWidget::calculateSensitivity(const QnVirtualCameraResourcePtr &camera) const
{
    QnMotionRegion region = camera->getMotionRegion(0);
    auto rects = region.getAllMotionRects();
    if (rects.empty())
        return QnMotionRegion::kDefaultSensitivity;

    // Assume motion region looks sane.
    return rects.begin().key();
}

void QnWearableMotionWidget::submitSensitivity(const QnVirtualCameraResourcePtr &camera, int sensitivity) const
{
    QnMotionRegion region;
    region.addRect(sensitivity, QRect(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight));
    camera->setMotionRegion(region, 0);
}
