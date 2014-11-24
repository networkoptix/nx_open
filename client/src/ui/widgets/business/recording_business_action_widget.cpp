#include "recording_business_action_widget.h"
#include "ui_recording_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

QnRecordingBusinessActionWidget::QnRecordingBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::RecordingBusinessActionWidget)
{
    ui->setupUi(this);

    for (int i = Qn::QualityLowest; i <= Qn::QualityHighest; i++) {
        ui->qualityComboBox->addItem(Qn::toDisplayString((Qn::StreamQuality)i), i);
    }

    ui->beforeLabel->setVisible(false);
    ui->beforeSpinBox->setVisible(false);
    ui->durationLabel->setVisible(false);
    ui->durationSpinBox->setVisible(false);

    connect(ui->qualityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->fpsSpinBox, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));
    connect(ui->fpsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
//    connect(ui->durationSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
//    connect(ui->beforeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->afterSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
}

QnRecordingBusinessActionWidget::~QnRecordingBusinessActionWidget()
{
}

void QnRecordingBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before,                 ui->qualityComboBox);
    setTabOrder(ui->qualityComboBox,    ui->fpsSpinBox);
    setTabOrder(ui->fpsSpinBox,         ui->afterSpinBox);
    setTabOrder(ui->afterSpinBox,       after);
}

void QnRecordingBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    int maxFps = 0;

    if (fields & QnBusiness::ActionResourcesField) {
        QnVirtualCameraResourceList cameras = model->actionResources().filtered<QnVirtualCameraResource>();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            maxFps = maxFps == 0 ? camera->getMaxFps() : qMax(maxFps, camera->getMaxFps());
        }

        ui->fpsSpinBox->setEnabled(maxFps > 0);
        ui->fpsSpinBox->setMaximum(maxFps);
        if (ui->fpsSpinBox->value() == 0 && maxFps > 0)
            ui->fpsSpinBox->setValue(maxFps);
        ui->fpsSpinBox->setMinimum(maxFps > 0 ? 1 : 0);
    }

    if (fields & QnBusiness::ActionParamsField) {

        QnBusinessActionParameters params = model->actionParams();

        int quality = ui->qualityComboBox->findData((int) params.streamQuality);
        if (quality >= 0)
            ui->qualityComboBox->setCurrentIndex(quality);

        ui->fpsSpinBox->setValue(params.fps);
        ui->afterSpinBox->setValue(params.recordAfter);
    }
}

void QnRecordingBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;

    params.fps = ui->fpsSpinBox->value();
    params.recordAfter = ui->afterSpinBox->value();
    params.streamQuality = (Qn::StreamQuality)ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt();
    model()->setActionParams(params);
}
