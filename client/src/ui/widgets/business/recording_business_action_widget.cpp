#include "recording_business_action_widget.h"
#include "ui_recording_business_action_widget.h"

#include <events/recording_business_action.h>

QnRecordingBusinessActionWidget::QnRecordingBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnRecordingBusinessActionWidget)
{
    ui->setupUi(this);

    for (int i = QnQualityLowest; i <= QnQualityHighest; i++) {
        ui->qualityComboBox->addItem(QnStreamQualityToString((QnStreamQuality)i), i);
    }

}

QnRecordingBusinessActionWidget::~QnRecordingBusinessActionWidget()
{
    delete ui;
}

void QnRecordingBusinessActionWidget::loadParameters(const QnBusinessParams &params) {
    int quality = ui->qualityComboBox->findData((int) BusinessActionParameters::getStreamQuality(params));
    if (quality >= 0)
        ui->qualityComboBox->setCurrentIndex(quality);
    ui->fpsSpinBox->setValue(BusinessActionParameters::getFps(params));
    ui->durationSpinBox->setValue(BusinessActionParameters::getRecordDuration(params));
}

QnBusinessParams QnRecordingBusinessActionWidget::parameters() const {
    QnBusinessParams params;

    BusinessActionParameters::setFps(&params, ui->fpsSpinBox->value());
    BusinessActionParameters::setRecordDuration(&params, ui->durationSpinBox->value());
    BusinessActionParameters::setStreamQuality(&params,
        (QnStreamQuality)ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt());
    return params;
}
