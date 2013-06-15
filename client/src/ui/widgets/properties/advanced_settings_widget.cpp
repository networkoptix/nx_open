#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"
#include "ui/style/skin.h"

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::AdvancedSettingsWidget),
    m_disableUpdate(false)
{
    ui->setupUi(this);
    ui->iconLabel->setPixmap(qnSkin->pixmap("warning.png"));

    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_ui_DataChanged()) );
    connect(ui->checkBoxDisableControl, SIGNAL(stateChanged(int)), this, SLOT(at_ui_DataChanged()) );

    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(at_restoreDefault()));
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{

}

QnSecurityCamResource::SecondaryStreamQuality QnAdvancedSettingsWidget::secondaryStreamQuality() const
{
    int val = ui->qualitySlider->value();
    bool dontChangeUsed = ui->qualitySlider->maximum() == 3;
    if (dontChangeUsed) {
        if (val == 0)
            return QnSecurityCamResource::SSQualityDontChange;
        val--;
    }
    if (val == 0)
        return QnSecurityCamResource::SSQualityLow;
    else if (val == 1)
        return QnSecurityCamResource::SSQualityMedium;
    else 
        return QnSecurityCamResource::SSQualityHigh;
}

Qt::CheckState QnAdvancedSettingsWidget::getCameraControl() const
{
    return ui->checkBoxDisableControl->checkState();
}

void QnAdvancedSettingsWidget::setQualitySlider(QnSecurityCamResource::SecondaryStreamQuality quality)
{
    int offset = ui->qualitySlider->maximum() - 2;
    if (quality == QnSecurityCamResource::SSQualityLow)
        ui->qualitySlider->setValue(0 + offset);
    else if (quality == QnSecurityCamResource::SSQualityHigh)
        ui->qualitySlider->setValue(2 + offset);
    else
        ui->qualitySlider->setValue(1 + offset);
}

void QnAdvancedSettingsWidget::updateFromResource(QnSecurityCamResourcePtr camera)
{
    m_disableUpdate = true;

    setKeepQualityVisible(false);
    setQualitySlider(camera->secondaryStreamQuality());
    ui->checkBoxDisableControl->setChecked(camera->isCameraControlDisabled());

    m_disableUpdate = false;
}

void QnAdvancedSettingsWidget::updateFromResources(QnVirtualCameraResourceList cameras)
{
    if (cameras.isEmpty())
        return;

    m_disableUpdate = true;

    bool sameQuality = true;
    bool sameControlState = true;
    QnSecurityCamResource::SecondaryStreamQuality quality = cameras[0]->secondaryStreamQuality();
    bool controlState = cameras[0]->isCameraControlDisabled();
    for (int i = 1; i < cameras.size(); ++i)
    {
        if (cameras[i]->secondaryStreamQuality() != quality)
            sameQuality = false;
        if (cameras[i]->isCameraControlDisabled() != controlState)
            sameControlState = false;
    }
    setKeepQualityVisible(!sameQuality);
    
    if (sameQuality)
        setQualitySlider(quality);
    else
        ui->qualitySlider->setValue(0);

    if (sameControlState)
        ui->checkBoxDisableControl->setChecked(controlState);
    else
        ui->checkBoxDisableControl->setCheckState(Qt::PartiallyChecked);

    m_disableUpdate = false;
}

void QnAdvancedSettingsWidget::setKeepQualityVisible(bool value)
{
    ui->keepQualityLabel->setVisible(value);
    ui->qualitySlider->setMaximum(2 + (value ? 1 : 0));
}

void QnAdvancedSettingsWidget::at_ui_DataChanged()
{
    if (!m_disableUpdate)
        emit dataChanged();
}

void QnAdvancedSettingsWidget::at_restoreDefault()
{
    setKeepQualityVisible(false);
    ui->checkBoxDisableControl->setChecked(false);
    ui->qualitySlider->setValue(1);
}
