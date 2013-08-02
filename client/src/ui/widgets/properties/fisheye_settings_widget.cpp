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

    connect(ui->angleSpinBox,     SIGNAL(valueChanged(double)), this, SLOT(at_angleDataChanged()));
    connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->horizontalRadioButton, SIGNAL(clicked(bool)), this, SLOT(at_dataChanged()));
    connect(ui->viewDownButton, SIGNAL(clicked(bool)), this, SLOT(at_dataChanged()));
    connect(ui->viewUpButton, SIGNAL(clicked(bool)), this, SLOT(at_dataChanged()));
}

QnFisheyeSettingsWidget::~QnFisheyeSettingsWidget()
{

}

void QnFisheyeSettingsWidget::updateFromResource(QnSecurityCamResourcePtr camera)
{
    if (!camera)
        return;
    m_silenseMode = true;

    m_dewarpingParams = camera->getDewarpingParams();
    switch (m_dewarpingParams.viewMode)
    {
        case DewarpingParams::Horizontal:
            ui->horizontalRadioButton->setChecked(true);
            break;
        case DewarpingParams::VerticalUp:
            ui->viewUpButton->setChecked(true);
            break;
        case DewarpingParams::VerticalDown:
            ui->viewDownButton->setChecked(true);
            break;
        default:
            Q_ASSERT_X( __LINE__, Q_FUNC_INFO, "Unsupported value");
    }

    ui->horizontalSlider->setValue(m_dewarpingParams.fovRot * 10);
     
    if (!m_silenseMode)
        emit dataChanged();

    m_silenseMode = false;
}

qreal QnFisheyeSettingsWidget::getAngle()
{
    return ui->horizontalSlider->value() / 10.0;
}

void QnFisheyeSettingsWidget::at_angleDataChanged()
{
    ui->horizontalSlider->setValue(ui->angleSpinBox->value()*10);
}

void QnFisheyeSettingsWidget::at_dataChanged()
{
    ui->angleSpinBox->blockSignals(true);
    ui->angleSpinBox->setValue(getAngle());
    m_dewarpingParams.fovRot = getAngle();

    if (ui->horizontalRadioButton->isChecked())
        m_dewarpingParams.viewMode = DewarpingParams::Horizontal;
    else if (ui->viewDownButton->isChecked())
        m_dewarpingParams.viewMode = DewarpingParams::VerticalDown;
    else
        m_dewarpingParams.viewMode = DewarpingParams::VerticalUp;

    if (!m_silenseMode)
        emit dataChanged();
    ui->angleSpinBox->blockSignals(false);
}

DewarpingParams QnFisheyeSettingsWidget::dewarpingParams() const
{
    return m_dewarpingParams;
}
