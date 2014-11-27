#include "onvif_imaging_settings.h"

#ifdef ENABLE_ONVIF

//#include "onvif/soapImagingBindingProxy.h"
#include "onvif/soapStub.h"

//
// class OnvifCameraSettingOperationAbstract
// 
QnAbstractOnvifImagingOperation::QnAbstractOnvifImagingOperation(ImagingSettingsResp* settings):
    m_settings(settings)
{}

QnAbstractOnvifImagingOperation::~QnAbstractOnvifImagingOperation() {}


QnFloatOnvifImagingOperation::QnFloatOnvifImagingOperation(ImagingSettingsResp* settings, std::function<float* (ImagingSettingsResp* settings)> path):
    QnAbstractOnvifImagingOperation(settings),
    m_path(path)
{}

QnFloatOnvifImagingOperation::~QnFloatOnvifImagingOperation() {}

bool QnFloatOnvifImagingOperation::get(QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;
    value = QString::number(*(m_path(m_settings)));
    return true;
}

bool QnFloatOnvifImagingOperation::set(const QString& value) const {
    if (!m_settings->ImagingSettings || !m_path(m_settings))
        return false;

    bool ok;
    float result = value.toFloat(&ok);
    if (!ok)
        return false;
    *(m_path(m_settings)) = result;
    return true;
}



const QString& CommonStringValues::ON = *(new QString(QLatin1String("On")));
const QString& CommonStringValues::OFF = *(new QString(QLatin1String("Off")));
const QString& CommonStringValues::SEPARATOR = *(new QString(QLatin1String(",")));

const OnvifCameraSettingOperationAbstract& OnvifCameraSettingOperationAbstract::EMPTY_OPERATION = *(new OnvifCameraSettingOperationEmpty());

static QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> > createOnvifCameraSettingOperationAbstract()
{
    QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> > tmp;

    tmp.insert(lit("Imaging_WhiteBalance_YbGain"),          QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingWhiteBalanceYbGainOperation()));
    tmp.insert(lit("Imaging_Exposure_Iris"),                  QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureIrisOperation()));
    tmp.insert(lit("Imaging_Exposure_ExposureTime"),         QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureExposureTimeOperation()));
    tmp.insert(lit("Imaging_WideDynamicRange_Mode"),        QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingWideDynamicRangeModeOperation()));
    tmp.insert(lit("Imaging_WhiteBalance_Mode"),             QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingWhiteBalanceModeOperation()));
    tmp.insert(lit("Imaging_Exposure_MaxGain"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMaxGainOperation()));
    tmp.insert(lit("Imaging_Exposure_MinExposureTime"),     QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMinExposureTimeOperation()));
    tmp.insert(lit("Imaging_BacklightCompensation_Level"),   QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingBacklightCompensationLevelOperation()));
    tmp.insert(lit("Imaging_Contrast"),                        QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingContrastOperation()));
    tmp.insert(lit("Imaging_Exposure_Priority"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposurePriorityOperation()));
    tmp.insert(lit("Imaging_Sharpness"),                       QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingSharpnessOperation()));
    tmp.insert(lit("Imaging_IrCutFilterMode"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingIrCutFilterModesOperation()));
    tmp.insert(lit("Imaging_WhiteBalance_YrGain"),          QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingWhiteBalanceYrGainOperation()));
    tmp.insert(lit("Imaging_Exposure_MinGain"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMinGainOperation()));
    tmp.insert(lit("Imaging_Exposure_Gain"),                  QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureGainOperation()));
    tmp.insert(lit("Imaging_Exposure_Mode"),                  QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureModeOperation()));
    tmp.insert(lit("Imaging_WideDynamicRange_Level"),       QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingWideDynamicRangeLevelOperation()));
    tmp.insert(lit("Imaging_Exposure_MaxIris"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMaxIrisOperation()));
    
    tmp.insert(lit("Imaging_Exposure_MinIris"),              QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMinIrisOperation()));
    tmp.insert(lit("Imaging_BacklightCompensation_Mode"),    QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingBacklightCompensationModeOperation()));
    tmp.insert(lit("Imaging_ColorSaturation"),                QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingColorSaturationOperation()));
    tmp.insert(lit("Imaging_Exposure_MaxExposureTime"),     QSharedPointer<OnvifCameraSettingOperationAbstract>(new ImagingExposureMaxExposureTimeOperation()));
    tmp.insert(lit("Maintenance_SystemReboot"),               QSharedPointer<OnvifCameraSettingOperationAbstract>(new MaintenanceSystemRebootOperation()));
    tmp.insert(lit("Maintenance_SoftSystemFactoryDefault"), QSharedPointer<OnvifCameraSettingOperationAbstract>(new MaintenanceSoftSystemFactoryDefaultOperation()));
    tmp.insert(lit("Maintenance_HardSystemFactoryDefault"), QSharedPointer<OnvifCameraSettingOperationAbstract>(new MaintenanceHardSystemFactoryDefaultOperation()));

    return tmp;
}



bool ImagingWhiteBalanceYbGainOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->WhiteBalance || !rangesResp.ImagingOptions->WhiteBalance->YbGain ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CbGain)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->WhiteBalance->YbGain->Min);
    output.setMax(rangesResp.ImagingOptions->WhiteBalance->YbGain->Max);

    output.setStep(calcStep(output.getMin(), output.getMax()));

    output.setCurrent(*(valsResp.ImagingSettings->WhiteBalance->CbGain));

    return true;
}

bool ImagingWhiteBalanceYbGainOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CbGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WhiteBalance->CbGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureIrisOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->Iris ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Iris)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->Iris->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->Iris->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->Iris));

    return true;
}

bool ImagingExposureIrisOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Iris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->Iris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureExposureTimeOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->ExposureTime ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->ExposureTime)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->ExposureTime->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->ExposureTime->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->ExposureTime));

    return true;
}

bool ImagingExposureExposureTimeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->ExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->ExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingWideDynamicRangeModeOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WideDynamicRange)
    {
        return false;
    }

    output.setMin(CommonStringValues::OFF);
    output.setMax(CommonStringValues::ON);
    output.setCurrent(valsResp.ImagingSettings->WideDynamicRange->Mode == onvifXsd__WideDynamicMode__OFF? CommonStringValues::OFF : CommonStringValues::ON);

    return true;
}

bool ImagingWideDynamicRangeModeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WideDynamicRange)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->WideDynamicRange->Mode),
        input.getCurrent() == CommonStringValues::OFF? onvifXsd__WideDynamicMode__OFF : onvifXsd__WideDynamicMode__ON, src);
}

const QString& ImagingWhiteBalanceModeOperation::AUTO_STR = *(new QString(QLatin1String("Auto")));
const QString& ImagingWhiteBalanceModeOperation::MANUAL_STR = *(new QString(QLatin1String("Manual")));
const QString& ImagingWhiteBalanceModeOperation::ALL_VALUES_STR = *(new QString(AUTO_STR + CommonStringValues::SEPARATOR + MANUAL_STR));

bool ImagingWhiteBalanceModeOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance)
    {
        return false;
    }

    output.setMin(ALL_VALUES_STR);
    output.setMax(ALL_VALUES_STR);
    output.setCurrent(valsResp.ImagingSettings->WhiteBalance->Mode == onvifXsd__WhiteBalanceMode__AUTO? AUTO_STR : MANUAL_STR);

    return true;
}

bool ImagingWhiteBalanceModeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->WhiteBalance->Mode),
        input.getCurrent() == AUTO_STR? onvifXsd__WhiteBalanceMode__AUTO : onvifXsd__WhiteBalanceMode__MANUAL, src);
}

bool ImagingExposureMaxGainOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MaxGain ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxGain)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MaxGain->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MaxGain->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MaxGain));

    return true;
}

bool ImagingExposureMaxGainOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMinExposureTimeOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MinExposureTime ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinExposureTime)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MinExposureTime->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MinExposureTime->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MinExposureTime));

    return true;
}

bool ImagingExposureMinExposureTimeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBacklightCompensationLevelOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->BacklightCompensation || !rangesResp.ImagingOptions->BacklightCompensation->Level ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation || !valsResp.ImagingSettings->BacklightCompensation->Level)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->BacklightCompensation->Level->Min);
    output.setMax(rangesResp.ImagingOptions->BacklightCompensation->Level->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->BacklightCompensation->Level));

    return true;
}

bool ImagingBacklightCompensationLevelOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation || !valsResp.ImagingSettings->BacklightCompensation->Level)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->BacklightCompensation->Level, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingContrastOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Contrast ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Contrast)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Contrast->Min);
    output.setMax(rangesResp.ImagingOptions->Contrast->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Contrast));

    return true;
}

bool ImagingContrastOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Contrast)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Contrast, static_cast<float>(static_cast<double>(input.getCurrent())), src);

    return true;
}

const QString& ImagingExposurePriorityOperation::LOW_NOISE_STR = *(new QString(QLatin1String("LowNoise")));
const QString& ImagingExposurePriorityOperation::FRAME_RATE_STR = *(new QString(QLatin1String("FrameRate")));
const QString& ImagingExposurePriorityOperation::ALL_VALUES_STR = *(new QString(LOW_NOISE_STR + CommonStringValues::SEPARATOR + FRAME_RATE_STR));

bool ImagingExposurePriorityOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Priority)
    {
        return false;
    }

    output.setMin(ALL_VALUES_STR);
    output.setMax(ALL_VALUES_STR);
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->Priority) == onvifXsd__ExposurePriority__LowNoise ? LOW_NOISE_STR : FRAME_RATE_STR);//

    return true;
}

bool ImagingExposurePriorityOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Priority)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->Priority, 
        input.getCurrent() == LOW_NOISE_STR? onvifXsd__ExposurePriority__LowNoise : onvifXsd__ExposurePriority__FrameRate, src);
}

bool ImagingSharpnessOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Sharpness ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Sharpness)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Sharpness->Min);
    output.setMax(rangesResp.ImagingOptions->Sharpness->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Sharpness));

    return true;
}

bool ImagingSharpnessOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Sharpness)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Sharpness, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

const QString& ImagingIrCutFilterModesOperation::AUTO_STR = *(new QString(QLatin1String("Auto")));
const QString& ImagingIrCutFilterModesOperation::ALL_VALUES_STR = *(new QString(CommonStringValues::OFF + CommonStringValues::SEPARATOR +
    CommonStringValues::ON + CommonStringValues::SEPARATOR + AUTO_STR));

bool ImagingIrCutFilterModesOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->IrCutFilter)
    {
        return false;
    }

    output.setMin(ALL_VALUES_STR);
    output.setMax(ALL_VALUES_STR);

    switch (*(valsResp.ImagingSettings->IrCutFilter))
    {
    case onvifXsd__IrCutFilterMode__ON:
        output.setCurrent(CommonStringValues::ON);
        break;

    case onvifXsd__IrCutFilterMode__OFF:
        output.setCurrent(CommonStringValues::OFF);
        break;

    case onvifXsd__IrCutFilterMode__AUTO:
        output.setCurrent(AUTO_STR);
        break;

    default:
        return false;
    }

    return true;
}

bool ImagingIrCutFilterModesOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->IrCutFilter)
    {
        return false;
    }

    QString val = input.getCurrent();
    onvifXsd__IrCutFilterMode enumVal;
    if (val == CommonStringValues::ON) {
        enumVal = onvifXsd__IrCutFilterMode__ON;
    } else if (val == CommonStringValues::OFF) {
        enumVal = onvifXsd__IrCutFilterMode__OFF;
    } else if (val == AUTO_STR) {
        enumVal = onvifXsd__IrCutFilterMode__AUTO;
    } else {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->IrCutFilter, enumVal, src);
}

bool ImagingWhiteBalanceYrGainOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->WhiteBalance || !rangesResp.ImagingOptions->WhiteBalance->YrGain ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CrGain)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->WhiteBalance->YrGain->Min);
    output.setMax(rangesResp.ImagingOptions->WhiteBalance->YrGain->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->WhiteBalance->CrGain));

    return true;
}

bool ImagingWhiteBalanceYrGainOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CrGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WhiteBalance->CrGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMinGainOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MinGain ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinGain)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MinGain->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MinGain->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MinGain));

    return true;
}

bool ImagingExposureMinGainOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureGainOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->Gain ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Gain)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->Gain->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->Gain->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->Gain));

    return true;
}

bool ImagingExposureGainOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Gain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->Gain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

const QString& ImagingExposureModeOperation::AUTO_STR = *(new QString(QLatin1String("Auto")));
const QString& ImagingExposureModeOperation::MANUAL_STR = *(new QString(QLatin1String("Manual")));
const QString& ImagingExposureModeOperation::ALL_VALUES_STR = *(new QString(AUTO_STR + CommonStringValues::SEPARATOR + MANUAL_STR));

bool ImagingExposureModeOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure)
    {
        return false;
    }

    output.setMin(ALL_VALUES_STR);
    output.setMax(ALL_VALUES_STR);
    output.setCurrent(valsResp.ImagingSettings->Exposure->Mode == onvifXsd__ExposureMode__AUTO? AUTO_STR : MANUAL_STR);

    return true;
}

bool ImagingExposureModeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->Exposure->Mode),
        input.getCurrent() == AUTO_STR? onvifXsd__ExposureMode__AUTO : onvifXsd__ExposureMode__MANUAL, src);
}

bool ImagingWideDynamicRangeLevelOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->WideDynamicRange || !rangesResp.ImagingOptions->WideDynamicRange->Level ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->WideDynamicRange || !valsResp.ImagingSettings->WideDynamicRange->Level)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->WideDynamicRange->Level->Min);
    output.setMax(rangesResp.ImagingOptions->WideDynamicRange->Level->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->WideDynamicRange->Level));

    return true;
}

bool ImagingWideDynamicRangeLevelOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WideDynamicRange || !valsResp.ImagingSettings->WideDynamicRange->Level)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WideDynamicRange->Level, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMaxIrisOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MaxIris ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxIris)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MaxIris->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MaxIris->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MaxIris));

    return true;
}

bool ImagingExposureMaxIrisOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxIris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxIris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBrightnessOperation::get(QString &value) const {

}

bool ImagingBrightnessOperation::set(const QString &value) const {

}

bool ImagingExposureMinIrisOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MinIris ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinIris)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MinIris->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MinIris->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MinIris));

    return true;
}

bool ImagingExposureMinIrisOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinIris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinIris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBacklightCompensationModeOperation::get(QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation)
    {
        return false;
    }

    output.setMin(CommonStringValues::OFF);
    output.setMax(CommonStringValues::ON);
    output.setCurrent(valsResp.ImagingSettings->BacklightCompensation->Mode == onvifXsd__BacklightCompensationMode__OFF? CommonStringValues::OFF : CommonStringValues::ON);

    return true;
}

bool ImagingBacklightCompensationModeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->BacklightCompensation->Mode),
        input.getCurrent() == CommonStringValues::OFF? onvifXsd__BacklightCompensationMode__OFF : onvifXsd__BacklightCompensationMode__ON, src);
}

bool ImagingColorSaturationOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->ColorSaturation ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->ColorSaturation)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->ColorSaturation->Min);
    output.setMax(rangesResp.ImagingOptions->ColorSaturation->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->ColorSaturation));

    return true;
}

bool ImagingColorSaturationOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->ColorSaturation)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->ColorSaturation, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMaxExposureTimeOperation::get(QString &value) const
{
    

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Exposure || !rangesResp.ImagingOptions->Exposure->MaxExposureTime ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxExposureTime)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Exposure->MaxExposureTime->Min);
    output.setMax(rangesResp.ImagingOptions->Exposure->MaxExposureTime->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Exposure->MaxExposureTime));

    return true;
}

bool ImagingExposureMaxExposureTimeOperation::set(const QString &value) const
{
    

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool MaintenanceSystemRebootOperation::get(QString &value /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceSystemRebootOperation::set(const QString &value /*input*/, bool /*reinit*/) const
{
    DeviceSoapWrapper* soapWrapper = src.getDeviceSoapWrapper();
    if (!soapWrapper) {
        return false;
    }

    RebootReq request;
    RebootResp response;

    int soapRes = soapWrapper->systemReboot(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "MaintenanceSystemRebootOperation::set: can't perform reboot on camera. UniqId: " << src.getUniqueId()
            << ". Reason: SOAP to endpoint " << soapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << soapWrapper->getLastError();
        return false;
    }

    return true;
}

bool MaintenanceSoftSystemFactoryDefaultOperation::get(QString &value /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceSoftSystemFactoryDefaultOperation::set(const QString &value /*input*/, bool /*reinit*/) const
{
    DeviceSoapWrapper* soapWrapper = src.getDeviceSoapWrapper();
    if (!soapWrapper) {
        return false;
    }

    FactoryDefaultReq request;
    FactoryDefaultResp response;

    int soapRes = soapWrapper->systemFactoryDefaultSoft(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "MaintenanceSoftSystemFactoryDefaultOperation::set: can't perform soft factory default on camera. UniqId: "
            << src.getUniqueId() << ". Reason: SOAP to endpoint " << soapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << soapWrapper->getLastError();
        return false;
    }

    return true;
}

bool MaintenanceHardSystemFactoryDefaultOperation::get(QString &value /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceHardSystemFactoryDefaultOperation::set(const QString &value /*input*/, bool /*reinit*/) const
{
    DeviceSoapWrapper* soapWrapper = src.getDeviceSoapWrapper();
    if (!soapWrapper) {
        return false;
    }

    FactoryDefaultReq request;
    FactoryDefaultResp response;

    int soapRes = soapWrapper->systemFactoryDefaultHard(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "MaintenanceSoftSystemFactoryDefaultOperation::set: can't perform hard factory default on camera. UniqId: "
            << src.getUniqueId() << ". Reason: SOAP to endpoint " << soapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << soapWrapper->getLastError();
        return false;
    }

    return true;
}


#endif

