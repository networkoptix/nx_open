#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

#include <ui/style/skin.h>
#include "core/resource/resource_type.h"

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::AdvancedSettingsWidget),
    m_disableUpdate(false),
    m_hasDualStreaming(false)
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

Qn::SecondStreamQuality QnAdvancedSettingsWidget::secondaryStreamQuality() const
{
    int val = ui->qualitySlider->value();
    if (isKeepQualityVisible()) {
        if (val == 0)
            return Qn::SSQualityNotDefined;
        val--;
    }
    if (val == Quality_Low)
        return Qn::SSQualityLow;
    else if (val == Quality_Medium)
        return Qn::SSQualityMedium;
    else 
        return Qn::SSQualityHigh;
}

Qt::CheckState QnAdvancedSettingsWidget::getCameraControl() const
{
    return ui->checkBoxDisableControl->checkState();
}

void QnAdvancedSettingsWidget::setQualitySlider(Qn::SecondStreamQuality quality)
{
    int offset = isKeepQualityVisible() ? 1 : 0;
    if (quality == Qn::SSQualityLow)
        ui->qualitySlider->setValue(Quality_Low + offset);
    else if (quality == Qn::SSQualityHigh)
        ui->qualitySlider->setValue(Quality_High + offset);
    else
        ui->qualitySlider->setValue(Quality_Medium + offset);
}

void QnAdvancedSettingsWidget::updateFromResource(QnSecurityCamResourcePtr camera)
{
    if (!camera)
        return;

    m_disableUpdate = true;

    setKeepQualityVisible(false);
    setQualitySlider(camera->secondaryStreamQuality());
    ui->checkBoxDisableControl->setChecked(camera->isCameraControlDisabled());
    m_hasDualStreaming = camera->hasDualStreaming();
    updateControlsState();
    m_disableUpdate = false;
    ui->labelWarn->setVisible(false);
    bool arecontCameraExists = false;
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    if (cameraType && cameraType->getManufacture() == lit("ArecontVision"))
        arecontCameraExists = true;
    ui->checkBoxDisableControl->setEnabled(!arecontCameraExists);
}

void QnAdvancedSettingsWidget::updateControlsState()
{
    bool val = m_hasDualStreaming && ui->checkBoxDisableControl->checkState() == Qt::Unchecked;
    ui->qualitySlider->setEnabled(val);
    ui->labelQuality->setEnabled(val);
    ui->keepQualityLabel->setEnabled(val);
    ui->labelLowQuality->setEnabled(val);
    ui->labelMedQuality->setEnabled(val);
    ui->labelHiQuality->setEnabled(val);
}

void QnAdvancedSettingsWidget::updateFromResources(QnVirtualCameraResourceList cameras)
{
    if (cameras.isEmpty())
        return;

    m_disableUpdate = true;

    bool sameQuality = true;
    bool sameControlState = true;
    Qn::SecondStreamQuality quality = cameras[0]->secondaryStreamQuality();
    bool controlState = cameras[0]->isCameraControlDisabled();
    m_hasDualStreaming = cameras[0]->hasDualStreaming();
    bool arecontCameraExists = false;
    bool mixedDualStreaming = false;
    for (int i = 1; i < cameras.size(); ++i)
    {
        if (cameras[i]->secondaryStreamQuality() != quality)
            sameQuality = false;
        if (cameras[i]->isCameraControlDisabled() != controlState)
            sameControlState = false;
        if (cameras[i]->hasDualStreaming() != m_hasDualStreaming)
            mixedDualStreaming = true;
        if (!cameras[i]->hasDualStreaming())
            m_hasDualStreaming = false;
        QnResourceTypePtr cameraType = qnResTypePool->getResourceType(cameras[i]->getTypeId());
        if (cameraType && cameraType->getManufacture() == lit("ArecontVision"))
            arecontCameraExists = true;
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

    updateControlsState();
    m_disableUpdate = false;
    ui->labelWarn->setVisible(mixedDualStreaming);
    ui->checkBoxDisableControl->setEnabled(!arecontCameraExists);
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
    updateControlsState();
    if (!m_disableUpdate)
        emit dataChanged();
}

void QnAdvancedSettingsWidget::at_restoreDefault()
{
    setKeepQualityVisible(false);
    ui->checkBoxDisableControl->setChecked(false);
    ui->qualitySlider->setValue(Quality_Medium);
}

