#include "legacy_wearable_motion_widget.h"
#include "ui_legacy_wearable_motion_widget.h"

#include <core/resource/camera_resource.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/core/motion/motion_grid.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>

using namespace nx::vms::client::desktop;

QnWearableMotionWidget::QnWearableMotionWidget(QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    ui(new Ui::WearableMotionWidget)
{
    ui->setupUi(this);

    for (int i = 1; i < QnMotionRegion::kSensitivityLevelCount; i++)
        ui->sensitivityComboBox->addItem(QString::number(i));

    m_aligner = new Aligner(this);
    m_aligner->addWidget(ui->sensitivityLabel);

    connect(ui->motionDetectionCheckBox, &QCheckBox::stateChanged, this,
        &QnWearableMotionWidget::changed);
    connect(ui->sensitivityComboBox, QnComboboxCurrentIndexChanged, this,
        &QnWearableMotionWidget::changed);

    connect(ui->motionDetectionCheckBox, &QCheckBox::stateChanged, this,
        &QnWearableMotionWidget::updateSensitivityEnabled);

    updateSensitivityEnabled();
}

QnWearableMotionWidget::~QnWearableMotionWidget()
{
}

Aligner* QnWearableMotionWidget::aligner() const
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
    setReadOnly(ui->sensitivityComboBox, readOnly);

    m_readOnly = readOnly;
}

void QnWearableMotionWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    ui->motionDetectionCheckBox->setChecked(camera->getMotionType() == Qn::MotionType::MT_SoftwareGrid);
    ui->sensitivityComboBox->setCurrentText(QString::number(calculateSensitivity(camera)));
}

void QnWearableMotionWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    NX_ASSERT(camera->getDefaultMotionType() == Qn::MotionType::MT_SoftwareGrid);
    camera->setMotionType(ui->motionDetectionCheckBox->isChecked() ? Qn::MotionType::MT_SoftwareGrid : Qn::MotionType::MT_NoMotion);
    submitSensitivity(camera, ui->sensitivityComboBox->currentText().toInt());
}

void QnWearableMotionWidget::updateSensitivityEnabled()
{
    bool enabled = ui->motionDetectionCheckBox->isChecked();

    ui->sensitivityLabel->setEnabled(enabled);
    ui->sensitivityComboBox->setEnabled(enabled);
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
    using nx::vms::client::core::MotionGrid;
    region.addRect(sensitivity, QRect(0, 0, MotionGrid::kWidth, MotionGrid::kHeight));
    camera->setMotionRegion(region, 0);
}
