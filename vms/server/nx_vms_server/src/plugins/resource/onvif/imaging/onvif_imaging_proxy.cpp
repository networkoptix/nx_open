#include "onvif_imaging_proxy.h"

#ifdef ENABLE_ONVIF

#include "onvif/soapImagingBindingProxy.h"
#include "onvif/soapStub.h"

#include <nx/utils/log/assert.h>

#define EXPAND(x) x

#define SPC2(a, b) \
    (((a) && ((a)->b)) ?    \
        ((a)->b) : nullptr) \

#define SPC3(a, b, c) \
    SPC2(SPC2(a, b), c)

#define SPC4(a, b, c, d) \
    SPC2(SPC3(a, b, c), d)

#define SPC5(a, b, c, d, e) \
    SPC2(SPC4(a, b, c, d), e)

#define SAFE_POINTER_CHAIN_SPECIALIZATION(_1, _2, _3, _4, _5, NAME, ...) NAME

#define SAFE_POINTER_CHAIN(...) \
    EXPAND( \
        EXPAND(SAFE_POINTER_CHAIN_SPECIALIZATION(__VA_ARGS__, SPC5, SPC4, SPC3, SPC2))(__VA_ARGS__))

#define CPC2(a, b) (a)->b
#define CPC3(a, b, c) (a)->b->c
#define CPC4(a, b, c, d) (a)->b->c->d
#define CPC5(a, b, c, d, e) (a)->b->c->d->e

#define CONCAT_TO_POINTER_CHAIN_SPECIALIZATION(_1, _2, _3, _4, _5, NAME, ...) NAME

#define CONCAT_TO_POINTER_CHAIN(...) \
    EXPAND( \
        EXPAND(CONCAT_TO_POINTER_CHAIN_SPECIALIZATION(__VA_ARGS__, CPC5, CPC4, CPC3, CPC2))(__VA_ARGS__))

//
// class QnOnvifImagingProxy
//
QnOnvifImagingProxy::QnOnvifImagingProxy(
    const SoapTimeouts& timeouts,
    const std::string& imagingUrl,
    const QString& login,
    const QString& passwd,
    const std::string& videoSrcToken,
    int _timeDrift)
    :
    m_rangesSoapWrapper(new ImagingSoapWrapper(timeouts, imagingUrl, login, passwd, _timeDrift)),
    m_valsSoapWrapper(new ImagingSoapWrapper(timeouts, imagingUrl, login, passwd, _timeDrift)),
    m_ranges(new ImagingOptionsResp()),
    m_values(new ImagingSettingsResp()),
    m_videoSrcToken(videoSrcToken),
    m_timeouts(timeouts)
{
    m_ranges->ImagingOptions = 0;
    m_values->ImagingSettings = 0;
}

QnOnvifImagingProxy::~QnOnvifImagingProxy()
{
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
//        NX_ASSERT(param.isValid());
        if (!param.isValid())
            return;
        param.setRange(range->Min, range->Max);
        if (!parameters.updateParameter(param))
            return;
        m_supportedOperations.insert(id, QnAbstractOnvifImagingOperationPtr(new QnFloatOnvifImagingOperation(m_values, path)));
    };

    auto registerEnumParameter = [this, &parameters](const QString &id, QnEnumOnvifImagingOperation::pathFunction path) {
        QnCameraAdvancedParameter param = parameters.getParameterById(id);
      //  NX_ASSERT(param.isValid());
        if (!param.isValid())
            return;
        m_supportedOperations.insert(id, QnAbstractOnvifImagingOperationPtr(new QnEnumOnvifImagingOperation(m_values, param.getRange(), path)));
    };

    auto blc = ranges->BacklightCompensation;
    if (blc) {
        registerEnumParameter(
            lit("ibcMode"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto blcPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, BacklightCompensation))
                    return reinterpret_cast<int*>(&blcPtr->Mode);
                return nullptr;
            });

        registerFloatParameter(lit("ibcLevel"), blc->Level,                     [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, BacklightCompensation, Level);});
    }

    auto exposure = ranges->Exposure;
    if (exposure) {
        registerEnumParameter(
            lit("ieMode"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto expPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure))
                    return reinterpret_cast<int*>(&expPtr->Mode);
                return nullptr;
            });

        registerEnumParameter(
            lit("iePriority"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto expPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure))
                    return reinterpret_cast<int*>(expPtr->Priority);
                return nullptr;
            });

        registerFloatParameter(lit("ieMinETime"),   exposure->MinExposureTime,  [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MinExposureTime);});
        registerFloatParameter(lit("ieMaxETime"),   exposure->MaxExposureTime,  [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MaxExposureTime);});
        registerFloatParameter(lit("ieETime"),      exposure->ExposureTime,     [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, ExposureTime);});

        registerFloatParameter(lit("ieMinGain"),    exposure->MinGain,          [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MinGain);});
        registerFloatParameter(lit("ieMaxGain"),    exposure->MaxGain,          [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MaxGain);});
        registerFloatParameter(lit("ieGain"),       exposure->Gain,             [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, Gain);});

        registerFloatParameter(lit("ieMinIris"),    exposure->MinIris,          [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MinIris);});
        registerFloatParameter(lit("ieMaxIris"),    exposure->MaxIris,          [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, MaxIris);});
        registerFloatParameter(lit("ieIris"),       exposure->Iris,             [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Exposure, Iris);});
    }

    auto focus = ranges->Focus;
    if (focus) {
        registerEnumParameter(
            lit("ifAuto"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto focusPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, Focus))
                    return reinterpret_cast<int*>(&focusPtr->AutoFocusMode);
                return nullptr;
            });

        registerFloatParameter(lit("ifSpeed"),      focus->DefaultSpeed,        [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Focus, DefaultSpeed);});
        registerFloatParameter(lit("ifNear"),       focus->NearLimit,           [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Focus, NearLimit);});
        registerFloatParameter(lit("ifFar"),        focus->FarLimit,            [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Focus, FarLimit);});
    }

    auto wdr = ranges->WideDynamicRange;
    if (wdr) {
        registerEnumParameter(
            lit("iwdrMode"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto wdrPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, WideDynamicRange))
                    return reinterpret_cast<int*>(&wdrPtr->Mode);
                return nullptr;
            });
        registerFloatParameter(lit("iwdrLevel"),    wdr->Level,                 [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, WideDynamicRange, Level);});
    }

    auto wb = ranges->WhiteBalance;
    if (wb) {
        registerEnumParameter(lit("iwbMode"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto wbPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings, WhiteBalance))
                    return reinterpret_cast<int*>(&wbPtr->Mode);
                return nullptr;
            });
        registerFloatParameter(lit("iwbYrGain"),    wb->YrGain,                 [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, WhiteBalance, CrGain);});
        registerFloatParameter(lit("iwbYbGain"),    wb->YbGain,                 [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, WhiteBalance, CbGain);});
    }

    if (!ranges->IrCutFilterModes.empty())
        registerEnumParameter(lit("iIrCut"),
            [](ImagingSettingsResp* settings) -> int*
            {
                if (auto irPtr = SAFE_POINTER_CHAIN(settings, ImagingSettings))
                    return reinterpret_cast<int*>(irPtr->IrCutFilter);
                return nullptr;
            });

    registerFloatParameter(lit("iBri"),             ranges->Brightness,         [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Brightness);});
    registerFloatParameter(lit("iCS"),              ranges->ColorSaturation,    [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, ColorSaturation);});
    registerFloatParameter(lit("iCon"),             ranges->Contrast,           [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Contrast);});
    registerFloatParameter(lit("iSha"),             ranges->Sharpness,          [](ImagingSettingsResp* settings){return SAFE_POINTER_CHAIN(settings, ImagingSettings, Sharpness);});

}

bool QnOnvifImagingProxy::makeSetRequest()
{
    QString endpoint = getImagingUrl();
    if (endpoint.isEmpty()) {
        return false;
    }

    ImagingSoapWrapper soapWrapper(m_timeouts, endpoint.toStdString(),
        login(), password(), timeDrift());
    SetImagingSettingsResp response;
    SetImagingSettingsReq request;
    NX_ASSERT(m_values);
    auto imagingSettings = *m_values->ImagingSettings;
    request.ImagingSettings = &imagingSettings;
    request.VideoSourceToken = m_videoSrcToken;

    int soapRes = soapWrapper.setImagingSettings(request, response);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifImagingProxy::makeSetRequest: can't save imaging options."
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->endpoint() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastErrorDescription();
        return false;
    }
    return true;
}

QString QnOnvifImagingProxy::getImagingUrl() const
{
    return m_rangesSoapWrapper->endpoint();
}

QString QnOnvifImagingProxy::login() const
{
    return m_rangesSoapWrapper->login();
}

QString QnOnvifImagingProxy::password() const
{
    return m_rangesSoapWrapper->password();
}

int QnOnvifImagingProxy::timeDrift() const
{
    return m_rangesSoapWrapper->timeDrift();
}

CameraDiagnostics::Result QnOnvifImagingProxy::loadRanges()
{
    ImagingOptionsReq rangesRequest;
    rangesRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_rangesSoapWrapper->getOptions(rangesRequest, *m_ranges);
    if (soapRes != SOAP_OK)
    {
        qWarning() << "QnOnvifImagingProxy::makeGetRequest: can't fetch imaging options."
            << "Reason: SOAP to endpoint " << m_rangesSoapWrapper->endpoint() << " failed. GSoap error code: "
            << soapRes << ". " << m_rangesSoapWrapper->getLastErrorDescription();
        return CameraDiagnostics::RequestFailedResult(lit("getOptions"), m_rangesSoapWrapper->getLastErrorDescription());
    }
    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnOnvifImagingProxy::loadValues(QnCameraAdvancedParamValueMap &values)
{
    ImagingSettingsReq valsRequest;
    valsRequest.VideoSourceToken = m_videoSrcToken;

    int soapRes = m_valsSoapWrapper->getImagingSettings(valsRequest, *m_values);
    if (soapRes != SOAP_OK) {
        qWarning() << "QnOnvifImagingProxy::makeGetRequest: can't fetch imaging settings."
            << "Reason: SOAP to endpoint " << m_valsSoapWrapper->endpoint() << " failed. GSoap error code: "
            << soapRes << ". " << m_valsSoapWrapper->getLastErrorDescription();
        return CameraDiagnostics::RequestFailedResult(lit("getImagingSettings"), m_valsSoapWrapper->getLastErrorDescription());
    }
    if ((m_values->ImagingSettings) && (m_values->ImagingSettings->Exposure)
        && (m_values->ImagingSettings->Exposure->Priority == nullptr))
    {
        auto soap = m_valsSoapWrapper->soap();
        m_values->ImagingSettings->Exposure->Priority = soap_new_onvifXsd__ExposurePriority(soap);
    }

    for (auto iter = m_supportedOperations.cbegin(); iter != m_supportedOperations.cend(); ++iter)
    {
        const QString id = iter.key();
        QString value;
        if (iter.value()->get(value))
            values[id] = value;
    }

    return CameraDiagnostics::NoErrorResult();
}

QSet<QString> QnOnvifImagingProxy::supportedParameters() const
{
    return m_supportedOperations.keys().toSet();
}

bool QnOnvifImagingProxy::setValue(const QString &id, const QString &value)
{
    if (!m_supportedOperations.contains(id))
        return false;
    QnAbstractOnvifImagingOperationPtr operation = m_supportedOperations[id];
    if (!operation->set(value))
        return false;

    return makeSetRequest();
}

#endif
