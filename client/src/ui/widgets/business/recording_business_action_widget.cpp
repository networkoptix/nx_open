#include "recording_business_action_widget.h"
#include "ui_recording_business_action_widget.h"

#include <events/recording_business_action.h>

#include <utils/common/scoped_value_rollback.h>

QnRecordingBusinessActionWidget::QnRecordingBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnRecordingBusinessActionWidget)
{
    ui->setupUi(this);

    for (int i = QnQualityLowest; i <= QnQualityHighest; i++) {
        ui->qualityComboBox->addItem(QnStreamQualityToString((QnStreamQuality)i), i);
    }
    ui->afterLabel->setVisible(false);
    ui->afterSpinBox->setVisible(false);
    ui->beforeLabel->setVisible(false);
    ui->beforeSpinBox->setVisible(false);

    connect(ui->qualityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->fpsSpinBox, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));
    connect(ui->durationSpinBox, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));
}

QnRecordingBusinessActionWidget::~QnRecordingBusinessActionWidget()
{
    delete ui;
}

/*
QString QnRecordingBusinessActionWidget::description() const {
    QString fmt = QLatin1String("%1 <span style=\"font-style:italic;\">(%2)</span>");
    QString recordStr = QObject::tr("Record");

    QString quality = QnStreamQualityToString(
        (QnStreamQuality)ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt());
    QString paramStr = ui->durationSpinBox->value() > 0
            ? QObject::tr("%1 fps, %2 quality, %3 sec max")
              .arg(ui->fpsSpinBox->value())
              .arg(quality)
              .arg(ui->durationSpinBox->value())
            : QObject::tr("%1 fps, %2 quality")
              .arg(ui->fpsSpinBox->value())
              .arg(quality);
    return fmt
            .arg(recordStr)
            .arg(paramStr);
}
*/


void QnRecordingBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {

        QnBusinessParams params = model->actionParams();

        int quality = ui->qualityComboBox->findData((int) BusinessActionParameters::getStreamQuality(params));
        if (quality >= 0)
            ui->qualityComboBox->setCurrentIndex(quality);
        ui->fpsSpinBox->setValue(BusinessActionParameters::getFps(params));
        ui->durationSpinBox->setValue(BusinessActionParameters::getRecordDuration(params));
    }

    //TODO: #GDM update on resource change
}

void QnRecordingBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;

    BusinessActionParameters::setFps(&params, ui->fpsSpinBox->value());
    BusinessActionParameters::setRecordDuration(&params, ui->durationSpinBox->value());
    BusinessActionParameters::setStreamQuality(&params,
        (QnStreamQuality)ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt());
    model()->setActionParams(params);
}
