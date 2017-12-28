#ifdef ENABLE_ONVIF

#include "digital_watchdog_resource.h"
#include "dw_ptz_controller.h"
#include "newdw_ptz_controller.h"
#include "dw_zoom_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <plugins/resource/onvif/onvif_stream_reader.h>
#include <common/static_common_module.h>
#include <plugins/utils/xml_request_helper.h>
#include <nx/utils/log/log.h>

static const int HTTP_PORT = 80;

// NOTE: This class using hardcoded XML reding/writeing intentionally, because CPro API may
// reject or crash on on requests, which are different from the original.
// TODO: Move out to a separate file as soon as it makes sense.
class QnDigitalWatchdogResource::CproApiClient
{
public:
    CproApiClient(QnDigitalWatchdogResource* resource) :
        m_resource(resource)
    {
        m_cacheExpiration = std::chrono::steady_clock::now();
    }

    bool updateVideoConfig()
    {
        if (m_cacheExpiration > std::chrono::steady_clock::now())
            return (bool)m_videoConfig;

        nx::plugins::utils::XmlRequestHelper requestHelper(
            m_resource->getUrl(), m_resource->getAuth(), nx::network::http::AuthType::authBasic);

        if (requestHelper.post(lit("GetVideoStreamConfig")))
            m_videoConfig = requestHelper.readRawBody();

        m_cacheExpiration = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        return (bool)m_videoConfig;
    }

    boost::optional<QStringList> getSupportedVedioCodecs(bool isPrimary)
    {
        auto stream = indexOfStream(isPrimary);
        if (stream == -1)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream on %2")
                .args(isPrimary, m_resource->getUrl()));
            return boost::none;
        }

        auto types = rangeOfTag("<encodeTypeCaps type=\"list\">", "</encodeTypeCaps>", stream);
        if (!types)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream capabilities on %2")
                .args(isPrimary, m_resource->getUrl()));
            return boost::none;
        }

        QStringList values;
        while (auto tag = rangeOfTag("<item>", "</item>", types->first, types->second))
        {
            values << QString::fromUtf8(m_videoConfig->mid(tag->first, tag->second));
            types->first = tag->first + tag->second;
        }

        if (values.isEmpty())
        {
            NX_DEBUG(this, lm("Unable to find %1 stream supported codecs on %2")
                .args(isPrimary, m_resource->getUrl()));
            return boost::none;
        }

        values.sort();
        return values;
    }

    boost::optional<QString> getVedioCodec(bool isPrimary)
    {
        auto stream = indexOfStream(isPrimary);
        if (stream == -1)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream on %2")
                .args(isPrimary, m_resource->getUrl()));
            return boost::none;
        }

        auto type = rangeOfTag("<encodeType>", "</encodeType>", stream);
        if (!type)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream codec on %2")
                .args(isPrimary, m_resource->getUrl()));
            return boost::none;
        }

        return QString::fromUtf8(m_videoConfig->mid(type->first, type->second));
    }

    bool setVedioCodec(bool isPrimary, const QString& value)
    {
        auto stream = indexOfStream(isPrimary);
        if (stream == -1)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream on %2")
                .args(isPrimary, m_resource->getUrl()));
            return false;
        }

        auto type = rangeOfTag("<encodeType>", "</encodeType>", stream);
        if (!type)
        {
            NX_DEBUG(this, lm("Unable to find %1 stream codec on %2")
                .args(isPrimary, m_resource->getUrl()));
            return false;
        }

        NX_DEBUG(this, lm("Set %1 stream codec to %2 on %3")
            .args(isPrimary, value, m_resource->getUrl()));

        nx::plugins::utils::XmlRequestHelper requestHelper(
            m_resource->getUrl(), m_resource->getAuth(), nx::network::http::AuthType::authBasic);

        m_videoConfig->replace(type->first, type->second, value.toUtf8());
        return requestHelper.post("SetVideoStreamConfig", *m_videoConfig);
    }

private:
    int indexOfStream(bool isPrimary)
    {
        return m_videoConfig->indexOf(isPrimary ? "<item id=\"1\"" : "<item id=\"3\"");
    }

    boost::optional<std::pair<int, int>> rangeOfTag(
        const QByteArray& openTag, const QByteArray& closeTag,
        int rangeBegin = 0, int rangeSize = 0)
    {
        auto start = m_videoConfig->indexOf(openTag, rangeBegin);
        if (start == -1 || (rangeSize && start >= rangeBegin + rangeSize))
            return boost::none;
        start += openTag.size();

        auto end = m_videoConfig->indexOf(closeTag, start);
        if (end == -1 || (rangeSize && end >= rangeBegin + rangeSize))
            return boost::none;

        return std::pair<int, int>{start, end - start};
    }

private:
    QnDigitalWatchdogResource* m_resource;
    boost::optional<QByteArray> m_videoConfig;
    std::chrono::steady_clock::time_point m_cacheExpiration;
};

bool modelHasZoom(const QString& cameraModel) {
    QString tmp = cameraModel.toLower();
    tmp = tmp.replace(QLatin1String(" "), QLatin1String(""));
    if (tmp.contains(QLatin1String("fd20")) || tmp.contains(QLatin1String("mpa20m")) || tmp.contains(QLatin1String("mc421"))) {
        return false;
    }
    return true;
}

QnDigitalWatchdogResource::QnDigitalWatchdogResource():
    QnPlOnvifResource(),
    m_hasZoom(false),
    m_cproApiClient(std::make_unique<CproApiClient>(this))
{
    setVendor(lit("Digital Watchdog"));
}

QnDigitalWatchdogResource::~QnDigitalWatchdogResource()
{
}

bool QnDigitalWatchdogResource::isCproChipset() const
{
    return getFirmware().startsWith("A");
}

bool QnDigitalWatchdogResource::useOnvifAdvancedParameterProviders() const
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    return isCproChipset() || resData.value<bool>(lit("forceOnvifAdvancedParameters"));
}

CLSimpleHTTPClient QnDigitalWatchdogResource::httpClient() const
{
    return CLSimpleHTTPClient(getHostAddress(), HTTP_PORT, getNetworkTimeout(), getAuth());
}

bool QnDigitalWatchdogResource::isDualStreamingEnabled(bool& unauth)
{
    if (m_appStopping)
        return false;

    CLSimpleHTTPClient http (getHostAddress(), HTTP_PORT, getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("/cgi-bin/getconfig.cgi?action=onvif"));
    if (status == CL_HTTP_SUCCESS)
    {
        QByteArray body;
        http.readAll(body);
        QList<QByteArray> lines = body.split(',');
        for (int i = 0; i < lines.size(); ++i)
        {
            if (lines[i].toLower().contains("onvif_stream_number"))
            {
                QList<QByteArray> params = lines[i].split(':');
                if (params.size() >= 2)
                {
                    int streams = params[1].trimmed().toInt();

                    return streams >= 2;
                }
            }
        }
    }
    else if (status == CL_HTTP_AUTH_REQUIRED)
    {
        unauth = true;
        setStatus(Qn::Unauthorized);
        return false;
    }

    return true; // ignore other error (for cameras with non standart HTTP port)
}

CameraDiagnostics::Result QnDigitalWatchdogResource::initInternal()
{
    bool unauth = false;
    if (!isDualStreamingEnabled(unauth) && unauth==false)
    {
        if (m_appStopping)
            return CameraDiagnostics::UnknownErrorResult();

        // The camera most likely is going to reset after enabling dual streaming
        enableOnvifSecondStream();
        return CameraDiagnostics::UnknownErrorResult();
    }
    disableB2FramesForActiDW();

    const CameraDiagnostics::Result result = QnPlOnvifResource::initInternal();

    return result;
}

void QnDigitalWatchdogResource::enableOnvifSecondStream()
{
    // The camera most likely is going to reset after enabling dual streaming
    CLSimpleHTTPClient http (getHostAddress(), HTTP_PORT, getNetworkTimeout(), getAuth());
    QByteArray request;
    request.append("onvif_stream_number=2&onvif_use_service=true&onvif_service_port=8032&");
    request.append("onvif_use_discovery=true&onvif_use_security=true&onvif_security_opts=63&onvif_use_sa=true&reboot=true");
    http.doPOST(QByteArray("/cgi-bin/onvifsetup.cgi"), QLatin1String(request));

    setStatus(Qn::Offline);
    // camera rebooting ....
}

bool QnDigitalWatchdogResource::disableB2FramesForActiDW()
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool isRebrendedActiCamera = resourceData.value<bool>(lit("isRebrendedActiCamera"), false);
    if (!isRebrendedActiCamera)
        return true;

    CLSimpleHTTPClient http(getHostAddress(), HTTP_PORT, getNetworkTimeout(), QAuthenticator());
    auto result = http.doGET(QString("/cgi-bin/system?User=%1&pwd=%2&RTP_B2=1").arg(getAuth().user()).arg(getAuth().password()));
    qDebug() << "disable RTP B2 frames for camera" << getHostAddress() << "result=" << result;
    return result == CL_HTTP_SUCCESS;
}

QnAbstractPtzController *QnDigitalWatchdogResource::createPtzControllerInternal()
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool useHttpPtz = resourceData.value<bool>(lit("dw-http-ptz"), false);
    QScopedPointer<QnAbstractPtzController> result;
    if (useHttpPtz)
    {
        result.reset(new QnNewDWPtzController(toSharedPointer(this)));
    }
    else {
        result.reset(new QnDwPtzController(toSharedPointer(this)));
        if(result->getCapabilities() == Ptz::NoPtzCapabilities) {
            result.reset();
            if(m_hasZoom)
                result.reset(new QnDwZoomPtzController(toSharedPointer(this)));
        }
    }
    return result.take();
}

bool QnDigitalWatchdogResource::loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    if (useOnvifAdvancedParameterProviders())
    {
        // DW CPro chipset (or something else that has uncompatible cgi interface).
        if (!base_type::loadAdvancedParametersTemplate(params))
            return false;
    }
    else if (resourceData.value<bool>(lit("dw-pravis-chipset")))
    {
        if (!loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw-pravis.xml")))
            return false;
    }
    else
    {
        if (!loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw.xml")))
            return false;
    }

    return loadCproAdvancedParameters(params);
}

static const QString kCproPrimaryVideoCodec = lit("cproPrimaryVideoCodec");
static const QString kCproSecondaryVideoCodec = lit("cproSecondaryVideoCodec");
static const QStringList kCproVideoCodecs{kCproPrimaryVideoCodec, kCproSecondaryVideoCodec};

bool QnDigitalWatchdogResource::loadCproAdvancedParameters(QnCameraAdvancedParams& params) const
{
    if (!m_cproApiClient->updateVideoConfig())
        return true;

    // Those paramiters are hardcoded as they are supposed to be applied on top of templates.
    // TODO: Makes sence to move into some template as soon as it supports paramiter merge.
    QnCameraAdvancedParamGroup streams;
    streams.name = lit("Video Streams");

    if (const auto codecs = m_cproApiClient->getSupportedVedioCodecs(/*isPrimary*/ true))
    {
        QnCameraAdvancedParameter codec;
        codec.id = kCproPrimaryVideoCodec;
        codec.name = lit("Codec");
        codec.dataType = QnCameraAdvancedParameter::DataType::Enumeration;
        codec.range = codecs->join(",");

        QnCameraAdvancedParamGroup group;
        group.name = lit("Primary");
        group.params.push_back(codec);
        streams.groups.push_back(group);
    }

    if (const auto codecs = m_cproApiClient->getSupportedVedioCodecs(/*isPrimary*/ false))
    {
        QnCameraAdvancedParameter codec;
        codec.id = kCproSecondaryVideoCodec;
        codec.name = lit("Codec");
        codec.dataType = QnCameraAdvancedParameter::DataType::Enumeration;
        codec.range = codecs->join(",");

        QnCameraAdvancedParamGroup group;
        group.name = lit("Secondary");
        group.params.push_back(codec);
        streams.groups.push_back(group);
    }

    params.groups.insert(params.groups.begin(), streams);
    return true;
}

void QnDigitalWatchdogResource::initAdvancedParametersProviders(QnCameraAdvancedParams &params)
{
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    if (useOnvifAdvancedParameterProviders())
    {
        base_type::initAdvancedParametersProviders(params);
        return;
    }

    if (resourceData.value<bool>(lit("dw-pravis-chipset")))
        m_cameraProxy.reset(new QnPravisCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
    else
        m_cameraProxy.reset(new QnWin4NetCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
    m_cameraProxy->setCameraAdvancedParams(params);
}

QSet<QString> QnDigitalWatchdogResource::calculateSupportedAdvancedParameters() const
{
    QSet<QString> result;
    if (m_cproApiClient->updateVideoConfig())
    {
        result.insert(kCproPrimaryVideoCodec);
        result.insert(kCproSecondaryVideoCodec);
    }

    result += base_type::calculateSupportedAdvancedParameters();
    if (useOnvifAdvancedParameterProviders())
        return result;

    for (const QnCameraAdvancedParamValue& value: m_cameraProxy->getParamsList())
        result.insert(value.id);

    return result;
}

void QnDigitalWatchdogResource::fetchAndSetAdvancedParameters() {
    base_type::fetchAndSetAdvancedParameters();
    if (useOnvifAdvancedParameterProviders())
        return;

    QString cameraModel = fetchCameraModel();
    m_hasZoom = modelHasZoom(cameraModel);
}

QString QnDigitalWatchdogResource::fetchCameraModel() {
    QAuthenticator auth = getAuth();
    // TODO: #vasilenko UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), getTimeDrift());

    DeviceInfoReq request;
    DeviceInfoResp response;

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK)
    {
        qWarning() << "QnDigitalWatchdogResource::fetchCameraModel: GetDeviceInformation SOAP to endpoint "
            << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError() << ". Only base (base for DW) advanced settings will be available for this camera.";

        return QString();
    }

    return QString::fromUtf8(response.Model.c_str());
}


bool QnDigitalWatchdogResource::loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap &values)
{
    QSet<QString> result;
    if (m_cproApiClient->updateVideoConfig())
    {
        if (const auto codec = m_cproApiClient->getVedioCodec(/*isPrimary*/ true))
            values.insert(kCproPrimaryVideoCodec, *codec);

        if (const auto codec = m_cproApiClient->getVedioCodec(/*isPrimary*/ false))
            values.insert(kCproSecondaryVideoCodec, *codec);
    }

    bool baseResult = base_type::loadAdvancedParamsUnderLock(values);

    if (!m_cameraProxy)
        return baseResult;

    values.appendValueList(m_cameraProxy->getParamsList());
    return true;
}

bool QnDigitalWatchdogResource::setAdvancedParameterUnderLock(
    const QnCameraAdvancedParameter &parameter, const QString &value)
{
    if (parameter.id == kCproPrimaryVideoCodec)
        return m_cproApiClient->setVedioCodec(/*isPrimary*/ true, value);

    if (parameter.id == kCproSecondaryVideoCodec)
        return m_cproApiClient->setVedioCodec(/*isPrimary*/ false, value);

    if (base_type::setAdvancedParameterUnderLock(parameter, value))
        return true;

    if (!m_cameraProxy)
        return false;

    QVector<QPair<QnCameraAdvancedParameter, QString>> params;
    params << QPair<QnCameraAdvancedParameter, QString>(parameter, value);

    return m_cameraProxy->setParams(params);
}

bool QnDigitalWatchdogResource::setAdvancedParametersUnderLock(
    const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    if (useOnvifAdvancedParameterProviders())
        return base_type::setAdvancedParametersUnderLock(values, result);

    bool success = true;
    QVector<QPair<QnCameraAdvancedParameter, QString>> moreParamsToProcess;
    for(const QnCameraAdvancedParamValue &value: values)
    {
        QnCameraAdvancedParameter parameter = m_advancedParameters.getParameterById(value.id);
        if (parameter.isValid())
        {
            if (parameter.id == kCproPrimaryVideoCodec || parameter.id == kCproSecondaryVideoCodec)
            {
                if (setAdvancedParameterUnderLock(parameter, value.value))
                    result << value;
                else
                    success = false;
            }
            else if (const auto baseResult
                = base_type::setAdvancedParameterUnderLock(parameter, value.value))
            {
                result << value;
            }
            else
            {
                moreParamsToProcess << QPair<QnCameraAdvancedParameter, QString>(
                    parameter, value.value);
            }
        }
        else
        {
            success = false;
        }
    }

    return success && m_cameraProxy->setParams(moreParamsToProcess, &result);
}

#endif //ENABLE_ONVIF
