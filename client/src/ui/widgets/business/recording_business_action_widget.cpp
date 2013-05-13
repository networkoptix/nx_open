#include "recording_business_action_widget.h"
#include "ui_recording_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

QnRecordingBusinessActionWidget::QnRecordingBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnRecordingBusinessActionWidget)
{
    ui->setupUi(this);

    for (int i = QnQualityLowest; i <= QnQualityHighest; i++) {
        ui->qualityComboBox->addItem(QnStreamQualityToString((QnStreamQuality)i), i);
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

void QnRecordingBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

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

        int quality = ui->qualityComboBox->findData((int) params.getStreamQuality());
        if (quality >= 0)
            ui->qualityComboBox->setCurrentIndex(quality);

        ui->fpsSpinBox->setValue(params.getFps());
        ui->afterSpinBox->setValue(params.getRecordAfter());
    }
}

void QnRecordingBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;

    params.setFps(ui->fpsSpinBox->value());
    params.setRecordAfter(ui->afterSpinBox->value());
    params.setStreamQuality((QnStreamQuality)ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt());
    model()->setActionParams(params);
}
