#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"

#include <ui/style/skin.h>
#include "core/resource/resource_type.h"

QnFisheyeSettingsWidget::QnFisheyeSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::FisheyeSettingsWidget),
    m_silenseMode(false)
{
    ui->setupUi(this);

    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->horizontalRadioButton, SIGNAL(clicked(bool)), this, SLOT(at_dataChanged()));
    connect(ui->verticalRadioButton, SIGNAL(clicked(bool)), this, SLOT(at_dataChanged()));
}

QnFisheyeSettingsWidget::~QnFisheyeSettingsWidget()
{

}

void QnFisheyeSettingsWidget::updateFromResource(QnSecurityCamResourcePtr camera)
{
    if (!camera)
        return;
    m_silenseMode = true;

    m_devorpingParams = camera->getDewarpingParams();
    ui->horizontalRadioButton->setChecked(m_devorpingParams.horizontalView);
    ui->verticalRadioButton->setChecked(!m_devorpingParams.horizontalView);
    ui->horizontalSlider->setValue(m_devorpingParams.fovRot * 10);
     
    if (!m_silenseMode)
        emit dataChanged();

    m_silenseMode = false;
}

qreal QnFisheyeSettingsWidget::getAngle()
{
    return ui->horizontalSlider->value() / 10.0;
}

void QnFisheyeSettingsWidget::at_dataChanged()
{
    ui->angleText->setText(QString::number(getAngle(), 'f', 1));
    m_devorpingParams.fovRot = getAngle();
    m_devorpingParams.horizontalView = ui->horizontalRadioButton->isChecked();
    if (!m_silenseMode)
        emit dataChanged();
}

DewarpingParams QnFisheyeSettingsWidget::devorpingParams() const
{
    return m_devorpingParams;
}
