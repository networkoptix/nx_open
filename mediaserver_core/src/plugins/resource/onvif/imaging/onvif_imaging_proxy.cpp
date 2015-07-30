#include "onvif_imaging_proxy.h"

#ifdef ENABLE_ONVIF

#include "onvif/soapImagingBindingProxy.h"
#include "onvif/soapStub.h"

//
// class QnOnvifImagingProxy
//
QnOnvifImagingProxy::QnOnvifImagingProxy(const std::string& imagingUrl,
                                                 const QString& login,
                                                 const QString& passwd, 
                                                 const std::string& videoSrcToken, 
                                                 int _timeDrift):
    m_rangesSoapWrapper(new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift)),
    m_valsSoapWrapper(new ImagingSoapWrapper(imagingUrl, login, passwd, _timeDrift)),
    m_ranges(new ImagingOptionsResp()),
    m_values(new ImagingSettingsResp()),
    m_videoSrcToken(videoSrcToken)
{
    m_ranges->ImagingOptions = 0;
    m_values->ImagingSettings = 0;
}

QnOnvifImagingProxy::~QnOnvifImagingProxy() {
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

void QnOnvifImagingProxy::initParameters(QnCameraAdvancedParams &parameters) {

    typedef onvifXsd__FloatRange floatRange;

    loadRanges();

    auto ranges = m_ranges->ImagingOptions;

    if (!ranges)
        return;

    auto registerFloatParameter = [this, &parameters](const QString &id, floatRange *range, QnFloatOnvifImagingOperation::pathFunction path) {
        if (!range)
            return;
        QnCameraAdvancedParameter param = parameters.getParameterById(id);
        Q_ASSERT(param.isValid());
        if (!param.isValid())
            return;
        param.setRange(range->Min, range->Max);
        if (!parameters.updateParameter(param))
            return;
        m_supportedOperations.insert(id, QnAbstractOnvifImagingOperationPtr(new QnFloatOnvifImagingOperation(m_values, path)));
    };

    auto registerEnumParameter = [this, &parameters](const QString &id, QnEnumOnvifImagingOperation::pathFunction path) {
        QnCameraAdvancedParameter param = parameters.getParameterById(id);
        Q_ASSERT(param.isValid());
        if (!param.isValid())
            return;
        m_supportedOperations.insert(id, QnAbstractOnvifImagingOperationPtr(new QnEnumOnvifImagingOperation(m_values, param.getRange(), path)));
    };

    auto blc = ranges->BacklightCompensation;
    if (blc) {
        registerEnumParameter(lit("ibcMode"),                                   [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->BacklightCompensation->Mode); });
        registerFloatParameter(lit("ibcLevel"), blc->Level,                     [](ImagingSettingsResp* settings){ return settings->ImagingSettings->BacklightCompensation->Level; });
    }

    auto exposure = ranges->Exposure;
    if (exposure) {
        registerEnumParameter(lit("ieMode"),                                    [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->Exposure->Mode); });
        registerEnumParameter(lit("iePriority"),                                [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->Exposure->Priority); });
        
        registerFloatParameter(lit("ieMinETime"),   exposure->MinExposureTime,  [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MinExposureTime; });
        registerFloatParameter(lit("ieMaxETime"),   exposure->MaxExposureTime,  [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MaxExposureTime; });
        registerFloatParameter(lit("ieETime"),      exposure->ExposureTime,     [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->ExposureTime; });

        registerFloatParameter(lit("ieMinGain"),    exposure->MinGain,          [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MinGain; });
        registerFloatParameter(lit("ieMaxGain"),    exposure->MaxGain,          [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MaxGain; });
        registerFloatParameter(lit("ieGain"),       exposure->Gain,             [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->Gain; });

        registerFloatParameter(lit("ieMinIris"),    exposure->MinIris,          [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MinIris; });
        registerFloatParameter(lit("ieMaxIris"),    exposure->MaxIris,          [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->MaxIris; });
        registerFloatParameter(lit("ieIris"),       exposure->Iris,             [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Exposure->Iris; });
    }
      
    auto focus = ranges->Focus;
    if (focus) {
        registerEnumParameter(lit("ifAuto"),                                    [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->Focus->AutoFocusMode); });

        registerFloatParameter(lit("ifSpeed"),      focus->DefaultSpeed,        [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Focus->DefaultSpeed; });
        registerFloatParameter(lit("ifNear"),       focus->NearLimit,           [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Focus->NearLimit; });
        registerFloatParameter(lit("ifFar"),        focus->FarLimit,            [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Focus->FarLimit; });
    }

    auto wdr = ranges->WideDynamicRange;
    if (wdr) {
        registerEnumParameter(lit("iwdrMode"),                                  [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->WideDynamicRange->Mode); });
        registerFloatParameter(lit("iwdrLevel"),    wdr->Level,                 [](ImagingSettingsResp* settings){ return settings->ImagingSettings->WideDynamicRange->Level; });
    }

    auto wb = ranges->WhiteBalance;
    if (wb) {
        registerEnumParameter(lit("iwbMode"),                                   [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->WhiteBalance->Mode); });
        registerFloatParameter(lit("iwbYrGain"),    wb->YrGain,                 [](ImagingSettingsResp* settings){ return settings->ImagingSettings->WhiteBalance->CrGain; });
        registerFloatParameter(lit("iwbYbGain"),    wb->YbGain,                 [](ImagingSettingsResp* settings){ return settings->ImagingSettings->WhiteBalance->CbGain; });
    }

    registerEnumParameter(lit("iIrCut"),                                        [](ImagingSettingsResp* settings){ return reinterpret_cast<int*>(&settings->ImagingSettings->IrCutFilter); });

    registerFloatParameter(lit("iBri"),             ranges->Brightness,         [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Brightness; });
    registerFloatParameter(lit("iCS"),              ranges->ColorSaturation,    [](ImagingSettingsResp* settings){ return settings->ImagingSettings->ColorSaturation; });
    registerFloatParameter(lit("iCon"),             ranges->Contrast,           [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Contrast; });   
    registerFloatParameter(lit("iSha"),             ranges->Sharpness,          [](ImagingSettingsResp* settings){ return settings->ImagingSettings->Sharpness; });

}

bool QnOnvifImagingProxy::makeSetRequest() {
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
        qWarning() << "QnOnvifImagingProxy::makeSetRequest: can't save imaging options."
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();
        return false;
    }
    return true;
}

QString QnOnvifImagingProxy::getImagingUrl() const
{
    return m_rangesSoapWrapper->getEndpointUrl();
}

QString QnOnvifImagingProxy::getLogin() const
{
    return m_rangesSoapWrapper->getLogin();
}

QString QnOnvifImagingProxy::getPassword() const
{
    return m_rangesSoapWrapper->getPassword();
}

int QnOnvifImagingProxy::getTimeDrift() const
{
    return m_rangesSoapWrapper->getTimeDrift();
}

CameraDiagnostics::Result QnOnvifImagingProxy::loadRanges() {
    ImagingOptionsReq rangesRequest;
    rangesRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_rangesSoapWrapper->getOptions(rangesRequest, *m_ranges);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifImagingProxy::makeGetRequest: can't fetch imaging options." 
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastError();
        return CameraDiagnostics::RequestFailedResult(lit("getOptions"), m_rangesSoapWrapper->getLastError());
    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifImagingProxy::loadValues(QnCameraAdvancedParamValueMap &values) {
    ImagingSettingsReq valsRequest;
    valsRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_valsSoapWrapper->getImagingSettings(valsRequest, *m_values);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifImagingProxy::makeGetRequest: can't fetch imaging settings." 
            << "Reason: SOAP to endpoint " << m_valsSoapWrapper->getEndpointUrl() << " failed. GSoap error code: "
            << soapRes << ". " << m_valsSoapWrapper->getLastError();
        return CameraDiagnostics::RequestFailedResult(lit("getImagingSettings"), m_valsSoapWrapper->getLastError());
    }

    for (auto iter = m_supportedOperations.cbegin(); iter != m_supportedOperations.cend(); ++iter) {
        const QString id = iter.key();
        QString value;
        if (iter.value()->get(value))
            values[id] = value;
    }

    return CameraDiagnostics::NoErrorResult();
}

QSet<QString> QnOnvifImagingProxy::supportedParameters() const {
    return m_supportedOperations.keys().toSet();
}


bool QnOnvifImagingProxy::setValue(const QString &id, const QString &value) {
    if (!m_supportedOperations.contains(id))
        return false;
    QnAbstractOnvifImagingOperationPtr operation = m_supportedOperations[id];
    if (!operation->set(value))
        return false;
    
    return makeSetRequest();
}

#endif
