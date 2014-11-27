#include "onvif_imaging_proxy.h"

#ifdef ENABLE_ONVIF

#include "onvif/soapImagingBindingProxy.h"
#include "onvif/soapStub.h"

#include <plugins/resource/onvif/imaging/onvif_imaging_settings.h>

//
// class OnvifCameraSettingsResp
//
OnvifCameraSettingsResp::OnvifCameraSettingsResp(const std::string& imagingUrl,
                                                 const QString& login,
                                                 const QString& passwd, 
                                                 const std::string& videoSrcToken, 
                                                 int _timeDrift):
    m_rangesSoapWrapper(new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift),
    m_valsSoapWrapper(new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift),
    m_ranges(new ImagingOptionsResp()),
    m_values(new ImagingSettingsResp()),
    m_videoSrcToken(videoSrcToken)
{
    m_ranges->ImagingOptions = 0;
    m_values->ImagingSettings = 0;
}

OnvifCameraSettingsResp::~OnvifCameraSettingsResp() {
    m_supportedOperations.clear();

    delete m_values;
    m_values = NULL;
    delete m_ranges;
    m_ranges = NULL;
    delete m_valsSoapWrapper;
    m_valsSoapWrapper = NULL;
    delete m_rangesSoapWrapper;
    m_rangesSoapWrapper = NULL;
}

void OnvifCameraSettingsResp::initParameters(QnCameraAdvancedParams &parameters) {
    loadRanges();
    if (!m_ranges->ImagingOptions || !m_values->ImagingSettings) {
        /* Clear template. */
        parameters.groups.clear();
        return;
    }

    if (m_ranges->ImagingOptions->Brightness && m_values->ImagingSettings->Brightness) {
        const QString id = lit("Imaging_Brightness");
        QnCameraAdvancedParameter brightness = parameters.getParameterById(id);
        brightness.min = QString::number(m_ranges->ImagingOptions->Brightness->Min);
        brightness.max = QString::number(m_ranges->ImagingOptions->Brightness->Max);
        m_supportedOperations.insert(id, QSharedPointer<QnAbstractOnvifImagingOperation>(
            new QnFloatOnvifImagingOperation(m_values, [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Brightness; })
            )
        );
    }
       

}

bool OnvifCameraSettingsResp::makeSetRequest() {
    QString endpoint = getImagingUrl();
    if (endpoint.isEmpty()) {
        return false;
    }

    QString login = getLogin();
    QString passwd = getPassword();

    ImagingSoapWrapper soapWrapper(endpoint.toStdString(), login, passwd, getTimeDrift());
    SetImagingSettingsResp response;
    SetImagingSettingsReq request;
    request.ImagingSettings = m_values->ImagingSettings;
    request.VideoSourceToken = m_videoSrcToken;

    int soapRes = soapWrapper.setImagingSettings(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeSetRequest: can't save imaging options."
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();
        return false;
    }
    return true;
}

QString OnvifCameraSettingsResp::getImagingUrl() const
{
    return m_rangesSoapWrapper->getEndpointUrl();
}

QString OnvifCameraSettingsResp::getLogin() const
{
    return m_rangesSoapWrapper->getLogin();
}

QString OnvifCameraSettingsResp::getPassword() const
{
    return m_rangesSoapWrapper->getPassword();
}

int OnvifCameraSettingsResp::getTimeDrift() const
{
    return m_rangesSoapWrapper->getTimeDrift();
}

CameraDiagnostics::Result OnvifCameraSettingsResp::loadRanges() {
    ImagingOptionsReq rangesRequest;
    rangesRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_rangesSoapWrapper->getOptions(rangesRequest, *m_ranges);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeGetRequest: can't fetch imaging options." 
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();
        return CameraDiagnostics::RequestFailedResult(lit("getOptions"), m_rangesSoapWrapper->getLastError());
    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result OnvifCameraSettingsResp::loadValues() {
    ImagingSettingsReq valsRequest;
    valsRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_valsSoapWrapper->getImagingSettings(valsRequest, *m_values);
    if (soapRes != SOAP_OK) {
        qWarning() << "OnvifCameraSettingsResp::makeGetRequest: can't fetch imaging settings." 
            << "Reason: SOAP to endpoint " << m_valsSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_valsSoapWrapper->getLastError();
        return CameraDiagnostics::RequestFailedResult(lit("getImagingSettings"), m_valsSoapWrapper->getLastError());
    }
    return CameraDiagnostics::NoErrorResult();
}

#endif
