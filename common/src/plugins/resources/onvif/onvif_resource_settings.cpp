#include "onvif_resource_settings.h"

//
// class OnvifCameraSettingsResp
//

OnvifCameraSettingsResp::OnvifCameraSettings(const std::string& endpoint, const std::string& login,
        const std::string& passwd, const std::string& videoSrcToken, const QString& uniqId):
    m_rangesSoapWrapper(endpoint, login, passwd),
    m_valsSoapWrapper(endpoint, login, passwd),
    m_videoSrcToken(videoSrcToken),
    m_uniqId(uniqId)
{
    m_rangesResponse.ImagingOptions = 0;
    m_valsResponse.ImagingSettings = 0;
}

bool OnvifCameraSettingsResp::isEmpty() const
{ 
    return !m_rangesResponse.ImagingOptions || !m_valsResponse.ImagingSettings;
}

bool OnvifCameraSettingsResp::makeRequest()
{
    ImagingOptionsReq rangesRequest;
    rangesRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_rangesSoapWrapper.getOptions(rangesRequest, m_rangesResponse);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeRequest: can't fetch imaging options. UniqId: " << m_uniqId
            << ". Reason: SOAP to endpoint " << m_rangesSoapWrapper.getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper.getLastError();
        return false;
    }

    ImagingSettingsReq valsRequest;
    valsRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_valsSoapWrapper.getImagingSettings(valsRequest, m_valsResponse);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeRequest: can't fetch imaging settings. UniqId: " << m_uniqId
            << ". Reason: SOAP to endpoint " << m_rangesSoapWrapper.getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper.getLastError();
        return false;
    }

    return true;
}

const ImagingOptionsResp& OnvifCameraSettingsResp::getRangesREsponse() const
{
  return m_rangesResponse;
}

const ImagingSettingsResp& OnvifCameraSettingsResp::getValsREsponse() const
{
  return m_valsResponse;
}

//
// class OnvifCameraSettingOperationAbstract
// 

QHash<QString, OnvifCameraSettingOperationAbstract*>& createOnvifCameraSettingOperationAbstract()
{
    return *(new QHash<QString, OnvifCameraSettingOperationAbstract*>());
}

const OnvifCameraSettingOperationAbstract::operations = createOnvifCameraSettingOperationAbstract();

//
// class OnvifCameraSetting: public CameraSetting
// 

OnvifCameraSetting::OnvifCameraSetting(const OnvifCameraSettingOperationAbstract& operation, const QString& id,
        const QString& name, WIDGET_TYPE type, const CameraSettingValue min, const CameraSettingValue max,
        const CameraSettingValue step, const CameraSettingValue current) :
    CameraSetting(id, name, type, min, max, step, current),
    m_operation(operation)
{

}
