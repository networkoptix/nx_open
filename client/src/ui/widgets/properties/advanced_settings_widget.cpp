#include "advanced_settings_widget.h"
#include "ui_advanced_settings_widget.h"

#include <ui/style/skin.h>

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

QnSecondaryStreamQuality QnAdvancedSettingsWidget::secondaryStreamQuality() const
{
    int val = ui->qualitySlider->value();
    if (isKeepQualityVisible()) {
        if (val == 0)
            return SSQualityNotDefined;
        val--;
    }
    if (val == Quality_Low)
        return SSQualityLow;
    else if (val == Quality_Medium)
        return SSQualityMedium;
    else 
        return SSQualityHigh;
}

Qt::CheckState QnAdvancedSettingsWidget::getCameraControl() const
{
    return ui->checkBoxDisableControl->checkState();
}

void QnAdvancedSettingsWidget::setQualitySlider(QnSecondaryStreamQuality quality)
{
    int offset = isKeepQualityVisible() ? 1 : 0;
    if (quality == SSQualityLow)
        ui->qualitySlider->setValue(Quality_Low + offset);
    else if (quality == SSQualityHigh)
        ui->qualitySlider->setValue(Quality_High + offset);
    else
        ui->qualitySlider->setValue(Quality_Medium + offset);
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
    QnSecondaryStreamQuality quality = cameras[0]->secondaryStreamQuality();
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

bool QnAdvancedSettingsWidget::isKeepQualityVisible() const
{
    return ui->keepQualityLabel->isVisible();
}

void QnAdvancedSettingsWidget::setKeepQualityVisible(bool value)
{
    ui->keepQualityLabel->setVisible(value);
    ui->qualitySlider->setMaximum(Quality_High + (value ? 1 : 0));
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
    ui->qualitySlider->setValue(Quality_Medium);
}

