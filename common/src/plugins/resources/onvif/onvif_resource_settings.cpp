#include "onvif_resource_settings.h"
#include "onvif/soapImagingBindingProxy.h"

enum onvifXsd__WideDynamicMode;

//
// class OnvifCameraSettingsResp
//

OnvifCameraSettingsResp::OnvifCameraSettingsResp(const std::string& deviceUrl, const std::string& imagingUrl,
        const std::string& login, const std::string& passwd, const std::string& videoSrcToken, const QString& uniqId, int _timeDrift):
    m_deviceSoapWrapper(deviceUrl.empty()? 0: new DeviceSoapWrapper(deviceUrl, login, passwd, _timeDrift)),
    m_rangesSoapWrapper(imagingUrl.empty()? 0: new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift)),
    m_valsSoapWrapper(imagingUrl.empty()? 0: new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift)),
    m_rangesResponse(new ImagingOptionsResp()),
    m_valsResponse(new ImagingSettingsResp()),
    m_videoSrcToken(videoSrcToken),
    m_uniqId(uniqId)
{
    m_rangesResponse->ImagingOptions = 0;
    m_valsResponse->ImagingSettings = 0;
}

OnvifCameraSettingsResp::~OnvifCameraSettingsResp()
{
    delete m_rangesSoapWrapper;
    delete m_valsSoapWrapper;
    delete m_rangesResponse;
    delete m_valsResponse;
}

bool OnvifCameraSettingsResp::isEmpty() const
{ 
    return !m_rangesResponse->ImagingOptions || !m_valsResponse->ImagingSettings;
}

bool OnvifCameraSettingsResp::makeGetRequest()
{
    if (!m_rangesSoapWrapper || !m_valsSoapWrapper) {
        return false;
    }

    ImagingOptionsReq rangesRequest;
    rangesRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_rangesSoapWrapper->getOptions(rangesRequest, *m_rangesResponse);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeGetRequest: can't fetch imaging options. UniqId: " << m_uniqId
            << ". Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();
        return false;
    }

    ImagingSettingsReq valsRequest;
    valsRequest.VideoSourceToken = m_videoSrcToken;

    soapRes = m_valsSoapWrapper->getImagingSettings(valsRequest, *m_valsResponse);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeGetRequest: can't fetch imaging settings. UniqId: " << m_uniqId
            << ". Reason: SOAP to endpoint " << m_valsSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_valsSoapWrapper->getLastError();
        return false;
    }

    return true;
}

bool OnvifCameraSettingsResp::makeSetRequest()
{
    if (!m_valsResponse) {
        return false;
    }

    QString endpoint = getImagingUrl();
    if (endpoint.isEmpty()) {
        return false;
    }

    QString login = getLogin();
    QString passwd = getPassword();

    ImagingSoapWrapper soapWrapper(endpoint.toStdString(), login.toStdString(), passwd.toStdString(), getTimeDrift());
    SetImagingSettingsResp response;
    SetImagingSettingsReq request;
    request.ImagingSettings = m_valsResponse->ImagingSettings;
    request.VideoSourceToken = m_videoSrcToken;

    int soapRes = soapWrapper.setImagingSettings(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeSetRequest: can't save imaging options. UniqId: " << m_uniqId
            << ". Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();

        //Restoring values from camera
        makeGetRequest();

        return false;
    }

    return true;
}

const ImagingOptionsResp& OnvifCameraSettingsResp::getRangesResponse() const
{
  return *m_rangesResponse;
}

const ImagingSettingsResp& OnvifCameraSettingsResp::getValsResponse() const
{
  return *m_valsResponse;
}

QString OnvifCameraSettingsResp::getImagingUrl() const
{
    return m_rangesSoapWrapper? m_rangesSoapWrapper->getEndpointUrl(): QString();
}

QString OnvifCameraSettingsResp::getLogin() const
{
    return m_rangesSoapWrapper? QString::fromLatin1(m_rangesSoapWrapper->getLogin()): QString();
}

QString OnvifCameraSettingsResp::getPassword() const
{
    return m_rangesSoapWrapper? QString::fromLatin1(m_rangesSoapWrapper->getPassword()): QString();
}

int OnvifCameraSettingsResp::getTimeDrift() const
{
    return m_rangesSoapWrapper? m_rangesSoapWrapper->getTimeDrift(): 0;
}

QString OnvifCameraSettingsResp::getUniqueId() const
{
    return m_uniqId;
}

DeviceSoapWrapper* OnvifCameraSettingsResp::getDeviceSoapWrapper()
{
    return m_deviceSoapWrapper;
}

//
// class OnvifCameraSettingOperationAbstract
// 

const QString& CommonStringValues::ON = *(new QString(QLatin1String("On")));
const QString& CommonStringValues::OFF = *(new QString(QLatin1String("Off")));
const QString& CommonStringValues::SEPARATOR = *(new QString(QLatin1String(",")));

const OnvifCameraSettingOperationAbstract& OnvifCameraSettingOperationAbstract::EMPTY_OPERATION = *(new OnvifCameraSettingOperationEmpty());

QHash<QString, OnvifCameraSettingOperationAbstract*>& createOnvifCameraSettingOperationAbstract()
{
    QHash<QString, OnvifCameraSettingOperationAbstract*>* tmp = new QHash<QString, OnvifCameraSettingOperationAbstract*>();

    tmp->insert(QString::fromLatin1("%%Imaging%%White Balance%%Yb Gain"),          new ImagingWhiteBalanceYbGainOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Iris"),                  new ImagingExposureIrisOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Exposure Time"),         new ImagingExposureExposureTimeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Wide Dynamic Range%%Mode"),        new ImagingWideDynamicRangeModeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%White Balance%%Mode"),             new ImagingWhiteBalanceModeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Max Gain"),              new ImagingExposureMaxGainOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Min Exposure Time"),     new ImagingExposureMinExposureTimeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Backlight Compensation%%Level"),   new ImagingBacklightCompensationLevelOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Contrast"),                        new ImagingContrastOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Priority"),              new ImagingExposurePriorityOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Sharpness"),                       new ImagingSharpnessOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Ir Cut Filter Mode"),              new ImagingIrCutFilterModesOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%White Balance%%Yr Gain"),          new ImagingWhiteBalanceYrGainOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Min Gain"),              new ImagingExposureMinGainOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Gain"),                  new ImagingExposureGainOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Mode"),                  new ImagingExposureModeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Wide Dynamic Range%%Level"),       new ImagingWideDynamicRangeLevelOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Max Iris"),              new ImagingExposureMaxIrisOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Brightness"),                      new ImagingBrightnessOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Min Iris"),              new ImagingExposureMinIrisOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Backlight Compensation%%Mode"),    new ImagingBacklightCompensationModeOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Color Saturation"),                new ImagingColorSaturationOperation());
    tmp->insert(QString::fromLatin1("%%Imaging%%Exposure%%Max Exposure Time"),     new ImagingExposureMaxExposureTimeOperation());
    tmp->insert(QString::fromLatin1("%%Maintenance%%System Reboot"),               new MaintenanceSystemRebootOperation());
    tmp->insert(QString::fromLatin1("%%Maintenance%%Soft System Factory Default"), new MaintenanceSoftSystemFactoryDefaultOperation());
    tmp->insert(QString::fromLatin1("%%Maintenance%%Hard System Factory Default"), new MaintenanceHardSystemFactoryDefaultOperation());

    return *tmp;
}

const QHash<QString, OnvifCameraSettingOperationAbstract*> OnvifCameraSettingOperationAbstract::operations = createOnvifCameraSettingOperationAbstract();

template<class T>
bool OnvifCameraSettingOperationAbstract::compareAndSendToCamera(T* field, const T val, OnvifCameraSettingsResp& src) const
{
    if (*field != val) {
        *field = val;
        return src.makeSetRequest();
    }

    return true;
}

float OnvifCameraSettingOperationAbstract::calcStep(double min, double max) const
{
    if (max - min < 1.0 + 0.1) {
        return 0.1f;
    }

    return 1.0f;
}

bool OnvifCameraSettingOperationEmpty::get(CameraSetting&, OnvifCameraSettingsResp&, bool) const
{
    return true;
}

bool OnvifCameraSettingOperationEmpty::set(const CameraSetting&, OnvifCameraSettingsResp&, bool) const
{
    return true;
}

bool ImagingWhiteBalanceYbGainOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWhiteBalanceYbGainOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CbGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WhiteBalance->CbGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureIrisOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureIrisOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Iris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->Iris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureExposureTimeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureExposureTimeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->ExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->ExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingWideDynamicRangeModeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWideDynamicRangeModeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWhiteBalanceModeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWhiteBalanceModeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->WhiteBalance->Mode),
        input.getCurrent() == AUTO_STR? onvifXsd__WhiteBalanceMode__AUTO : onvifXsd__WhiteBalanceMode__MANUAL, src);
}

bool ImagingExposureMaxGainOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMaxGainOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMinExposureTimeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMinExposureTimeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBacklightCompensationLevelOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingBacklightCompensationLevelOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation || !valsResp.ImagingSettings->BacklightCompensation->Level)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->BacklightCompensation->Level, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingContrastOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingContrastOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposurePriorityOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Priority)
    {
        return false;
    }

    output.setMin(ALL_VALUES_STR);
    output.setMax(ALL_VALUES_STR);
    output.setCurrent(valsResp.ImagingSettings->WhiteBalance->Mode == onvifXsd__ExposurePriority__LowNoise? LOW_NOISE_STR : FRAME_RATE_STR);

    return true;
}

bool ImagingExposurePriorityOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->Priority)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->Priority, 
        input.getCurrent() == LOW_NOISE_STR? onvifXsd__ExposurePriority__LowNoise : onvifXsd__ExposurePriority__FrameRate, src);
}

bool ImagingSharpnessOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingSharpnessOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingIrCutFilterModesOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingIrCutFilterModesOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWhiteBalanceYrGainOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWhiteBalanceYrGainOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WhiteBalance || !valsResp.ImagingSettings->WhiteBalance->CrGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WhiteBalance->CrGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMinGainOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMinGainOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinGain)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinGain, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureGainOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureGainOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureModeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureModeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->Exposure->Mode),
        input.getCurrent() == AUTO_STR? onvifXsd__ExposureMode__AUTO : onvifXsd__ExposureMode__MANUAL, src);
}

bool ImagingWideDynamicRangeLevelOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingWideDynamicRangeLevelOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->WideDynamicRange || !valsResp.ImagingSettings->WideDynamicRange->Level)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->WideDynamicRange->Level, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMaxIrisOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMaxIrisOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxIris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxIris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBrightnessOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingOptionsResp& rangesResp = src.getRangesResponse();
    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!rangesResp.ImagingOptions || !rangesResp.ImagingOptions->Brightness ||
        !valsResp.ImagingSettings || !valsResp.ImagingSettings->Brightness)
    {
        return false;
    }

    output.setMin(rangesResp.ImagingOptions->Brightness->Min);
    output.setMax(rangesResp.ImagingOptions->Brightness->Max);
    output.setStep(calcStep(output.getMin(), output.getMax()));
    output.setCurrent(*(valsResp.ImagingSettings->Brightness));

    return true;
}

bool ImagingBrightnessOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Brightness)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Brightness, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMinIrisOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMinIrisOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MinIris)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MinIris, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingBacklightCompensationModeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingBacklightCompensationModeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->BacklightCompensation)
    {
        return false;
    }

    return compareAndSendToCamera(&(valsResp.ImagingSettings->BacklightCompensation->Mode),
        input.getCurrent() == CommonStringValues::OFF? onvifXsd__BacklightCompensationMode__OFF : onvifXsd__BacklightCompensationMode__ON, src);
}

bool ImagingColorSaturationOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingColorSaturationOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->ColorSaturation)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->ColorSaturation, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool ImagingExposureMaxExposureTimeOperation::get(CameraSetting& output, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

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

bool ImagingExposureMaxExposureTimeOperation::set(const CameraSetting& input, OnvifCameraSettingsResp& src, bool reinit) const
{
    if (!reinitSrc(src, reinit)) return false;

    const ImagingSettingsResp& valsResp = src.getValsResponse();

    if (!valsResp.ImagingSettings || !valsResp.ImagingSettings->Exposure || !valsResp.ImagingSettings->Exposure->MaxExposureTime)
    {
        return false;
    }

    return compareAndSendToCamera(valsResp.ImagingSettings->Exposure->MaxExposureTime, static_cast<float>(static_cast<double>(input.getCurrent())), src);
}

bool MaintenanceSystemRebootOperation::get(CameraSetting& /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceSystemRebootOperation::set(const CameraSetting& /*input*/, OnvifCameraSettingsResp& src, bool /*reinit*/) const
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

bool MaintenanceSoftSystemFactoryDefaultOperation::get(CameraSetting& /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceSoftSystemFactoryDefaultOperation::set(const CameraSetting& /*input*/, OnvifCameraSettingsResp& src, bool /*reinit*/) const
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

bool MaintenanceHardSystemFactoryDefaultOperation::get(CameraSetting& /*output*/, OnvifCameraSettingsResp& /*src*/, bool /*reinit*/) const
{
    return true;
}

bool MaintenanceHardSystemFactoryDefaultOperation::set(const CameraSetting& /*input*/, OnvifCameraSettingsResp& src, bool /*reinit*/) const
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


//
// class OnvifCameraSetting: public CameraSetting
// 

OnvifCameraSetting::OnvifCameraSetting(const OnvifCameraSettingOperationAbstract& operation, const QString& id,
        const QString& name, WIDGET_TYPE type, const QString& query, const QString& description, const CameraSettingValue min,
        const CameraSettingValue max, const CameraSettingValue step, const CameraSettingValue current) :
    CameraSetting(id, name, type, query, description, min, max, step, current),
    m_operation(&operation)
{

}

OnvifCameraSetting& OnvifCameraSetting::operator=(const OnvifCameraSetting& rhs)
{
    CameraSetting::operator=(rhs);
    m_operation = rhs.m_operation;

    return *this;
}

//
// class OnvifCameraSettingReader
//

const QString& OnvifCameraSettingReader::IMAGING_GROUP_NAME = *(new QString(QLatin1String("%%Imaging")));
const QString& OnvifCameraSettingReader::MAINTENANCE_GROUP_NAME = *(new QString(QLatin1String("%%Maintenance")));

OnvifCameraSettingReader::OnvifCameraSettingReader(OnvifCameraSettingsResp& onvifSettings):
    CameraSettingReader(QString::fromLatin1("ONVIF")),
    m_settings(onvifSettings)
{

}

OnvifCameraSettingReader::~OnvifCameraSettingReader()
{

}

bool OnvifCameraSettingReader::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    if (m_settings.isEmpty() && id == IMAGING_GROUP_NAME)
    {
        //Group is enabled by default, to disable we should make a record, which equals empty object
        m_settings.getCameraSettings().insert(id, OnvifCameraSetting());

        return false;
    }

    return true;
}

bool OnvifCameraSettingReader::isParamEnabled(const QString& /*id*/, const QString& parentId)
{
    if (parentId.startsWith(MAINTENANCE_GROUP_NAME)) {
        //Maintenance group contains only buttons and so we don't need to create object with
        //values, so returning false to prevent parsing param values. 
        //But buttons themselves will be enabled
        return false;
    }

    return true;
}

void OnvifCameraSettingReader::paramFound(const CameraSetting& value, const QString& /*parentId*/)
{
    QString id = value.getId();
    QHash<QString, OnvifCameraSettingOperationAbstract*>::ConstIterator it;

    switch(value.getType())
    {
        case CameraSetting::OnOff: case CameraSetting::MinMaxStep: case CameraSetting::Enumeration:
            it = OnvifCameraSettingOperationAbstract::operations.find(id);
            if (it == OnvifCameraSettingOperationAbstract::operations.end()) {
                //Operations must be defined for all settings
                Q_ASSERT(false);
            }
            m_settings.getCameraSettings().insert(id, OnvifCameraSetting(*(it.value()), id, value.getName(),
                value.getType(), value.getQuery(), value.getDescription(), value.getMin(), value.getMax(), value.getStep()));
            return;

        case CameraSetting::Button:
            //If absent = enabled
            return;

        default:
            //XML must contain only valid types
            Q_ASSERT(false);
            return;
    }
}

void OnvifCameraSettingReader::cleanDataOnFail()
{
    m_settings.getCameraSettings().clear();
}

void OnvifCameraSettingReader::parentOfRootElemFound(const QString& /*parentId*/)
{

}
