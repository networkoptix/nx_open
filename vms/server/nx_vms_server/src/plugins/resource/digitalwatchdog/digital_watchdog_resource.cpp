#ifdef ENABLE_ONVIF

#include "digital_watchdog_resource.h"
#include "dw_ptz_controller.h"
#include "newdw_ptz_controller.h"
#include "dw_zoom_ptz_controller.h"

#include <onvif/soapDeviceBindingProxy.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <plugins/resource/onvif/onvif_stream_reader.h>
#include <camera/camera_pool.h>
#include <utils/media/av_codec_helper.h>
#include <plugins/utils/multisensor_data_provider.h>

#include <nx/vms/server/resource/camera.h>
#include <plugins/resource/digitalwatchdog/helpers.h>

bool modelHasZoom(const QString& cameraModel) {
    QString tmp = cameraModel.toLower();
    tmp = tmp.replace(QLatin1String(" "), QLatin1String(""));
    if (tmp.contains(QLatin1String("fd20")) || tmp.contains(QLatin1String("mpa20m")) || tmp.contains(QLatin1String("mc421"))) {
        return false;
    }
    return true;
}

QnDigitalWatchdogResource::QnDigitalWatchdogResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule),
    m_hasZoom(false),
    m_cproApiClient(std::make_unique<CproApiClient>(this))
{
    setVendor(lit("Digital Watchdog"));

    NX_VERBOSE(this, "Created: %1", getId());
}

QnDigitalWatchdogResource::~QnDigitalWatchdogResource()
{
    NX_VERBOSE(this, "Destroyed: %1", getId());
}

bool QnDigitalWatchdogResource::isCproChipset() const
{
    return getFirmware().startsWith("A");
}

bool QnDigitalWatchdogResource::useOnvifAdvancedParameterProviders() const
{
    return isCproChipset() || resourceData().value<bool>(lit("forceOnvifAdvancedParameters"));
}

std::unique_ptr<CLSimpleHTTPClient> QnDigitalWatchdogResource::httpClient() const
{
    return std::make_unique<CLSimpleHTTPClient>(
        getHostAddress(), nx::network::http::DEFAULT_HTTP_PORT, getNetworkTimeout(), getAuth());
}

bool QnDigitalWatchdogResource::isDualStreamingEnabled(bool& unauth)
{
    if (commonModule()->isNeedToStop())
        return false;

    auto http = httpClient();
    CLHttpStatus status = http->doGET(QByteArray("/cgi-bin/getconfig.cgi?action=onvif"));
    if (status == CL_HTTP_SUCCESS)
    {
        QByteArray body;
        http->readAll(body);
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

CameraDiagnostics::Result QnDigitalWatchdogResource::initializeCameraDriver()
{
    if (getRole() != Role::subchannel)
    {
        bool unauth = false;
        if (!isDualStreamingEnabled(unauth) && unauth == false)
        {
            if (commonModule()->isNeedToStop())
                return CameraDiagnostics::UnknownErrorResult();

            // The camera most likely is going to reset after enabling dual streaming
            enableOnvifSecondStream();
            return CameraDiagnostics::UnknownErrorResult();
        }
        disableB2FramesForActiDW();
    }
    const CameraDiagnostics::Result result = QnPlOnvifResource::initializeCameraDriver();
    return result;
}

void QnDigitalWatchdogResource::enableOnvifSecondStream()
{
    // The camera most likely is going to reset after enabling dual streaming
    auto http = httpClient();
    QByteArray request;
    request.append("onvif_stream_number=2&onvif_use_service=true&onvif_service_port=8032&");
    request.append("onvif_use_discovery=true&onvif_use_security=true&onvif_security_opts=63&onvif_use_sa=true&reboot=true");
    http->doPOST(QByteArray("/cgi-bin/onvifsetup.cgi"), QLatin1String(request));

    setStatus(Qn::Offline);
    // camera rebooting ....
}

bool QnDigitalWatchdogResource::disableB2FramesForActiDW()
{
    bool isRebrendedActiCamera = resourceData().value<bool>(lit("isRebrendedActiCamera"), false);
    if (!isRebrendedActiCamera)
        return true;

    CLSimpleHTTPClient http(
        getHostAddress(), nx::network::http::DEFAULT_HTTP_PORT, getNetworkTimeout(), QAuthenticator());

    auto result = http.doGET(QString("/cgi-bin/system?User=%1&pwd=%2&RTP_B2=1")
        .arg(getAuth().user()).arg(getAuth().password()));

    qDebug() << "disable RTP B2 frames for camera" << getHostAddress() << "result=" << result;
    return result == CL_HTTP_SUCCESS;
}

QnAbstractPtzController *QnDigitalWatchdogResource::createPtzControllerInternal() const
{
    bool useHttpPtz = resourceData().value<bool>(lit("dw-http-ptz"), false);
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

bool QnDigitalWatchdogResource::loadAdvancedParametersTemplate(
    QnCameraAdvancedParams& params) const
{
    if (useOnvifAdvancedParameterProviders())
    {
        // DW CPro chipset (or something else that has incompatible cgi interface).
        if (!base_type::loadAdvancedParametersTemplate(params))
            return false;
    }
    else if (resourceData().value<bool>(lit("dw-pravis-chipset")))
    {
        if (!loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw-pravis.xml")))
            return false;
    }
    else
    {
        if (!loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw.xml")))
            return false;
    }

    return true;
}

static const QString kCproPrimaryVideoCodec = lit("cproPrimaryVideoCodec");
static const QString kCproSecondaryVideoCodec = lit("cproSecondaryVideoCodec");
static const QStringList kCproParameters{kCproPrimaryVideoCodec, kCproSecondaryVideoCodec};

void QnDigitalWatchdogResource::initAdvancedParametersProvidersUnderLock(QnCameraAdvancedParams &params)
{
    if (useOnvifAdvancedParameterProviders())
    {
        base_type::initAdvancedParametersProvidersUnderLock(params);
        return;
    }

    if (resourceData().value<bool>(lit("dw-pravis-chipset")))
        m_cameraProxy.reset(new QnPravisCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
    else
        m_cameraProxy.reset(new QnWin4NetCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
    m_cameraProxy->setCameraAdvancedParams(params);
}

QSet<QString> QnDigitalWatchdogResource::calculateSupportedAdvancedParameters() const
{
    QSet<QString> result = base_type::calculateSupportedAdvancedParameters();
    if (useOnvifAdvancedParameterProviders())
        return result;

    if (!m_cameraProxy)
        return result;

    for (const QnCameraAdvancedParamValue& value: m_cameraProxy->getParamsList())
        result.insert(value.id);

    return result;
}

void QnDigitalWatchdogResource::fetchAndSetAdvancedParameters()
{
    base_type::fetchAndSetAdvancedParameters();
    if (useOnvifAdvancedParameterProviders())
        return;

    QString cameraModel = fetchCameraModel();
    m_hasZoom = modelHasZoom(cameraModel);
}

QString QnDigitalWatchdogResource::fetchCameraModel()
{
    QAuthenticator auth = getAuth();
    // TODO: #vasilenko UTF unuse StdString
    DeviceSoapWrapper soapWrapper(
        onvifTimeouts(),
        getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), getTimeDrift());

    DeviceInfoReq request;
    DeviceInfoResp response;

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK)
    {
        NX_DEBUG(this, makeSoapFailMessage(
            soapWrapper, __func__, "GetDeviceInformation", soapRes));
        return QString();
    }

    return QString::fromUtf8(response.Model.c_str());
}

bool QnDigitalWatchdogResource::loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap& values)
{
    bool baseResult = base_type::loadAdvancedParamsUnderLock(values);
    if (!m_cameraProxy)
        return baseResult;

    values.appendValueList(m_cameraProxy->getParamsList());
    return true;
}

bool QnDigitalWatchdogResource::setAdvancedParameterUnderLock(
    const QnCameraAdvancedParameter& parameter, const QString& value)
{
    if (base_type::setAdvancedParameterUnderLock(parameter, value))
        return true;

    if (!m_cameraProxy)
        return false;

    QVector<QPair<QnCameraAdvancedParameter, QString>> params;
    params << QPair<QnCameraAdvancedParameter, QString>(parameter, value);

    return m_cameraProxy->setParams(params);
}

bool QnDigitalWatchdogResource::setAdvancedParametersUnderLock(
    const QnCameraAdvancedParamValueList& values, QnCameraAdvancedParamValueList& result)
{
    if (useOnvifAdvancedParameterProviders())
        return base_type::setAdvancedParametersUnderLock(values, result);

    bool success = true;
    QVector<QPair<QnCameraAdvancedParameter, QString>> moreParamsToProcess;
    for(const QnCameraAdvancedParamValue &value: values)
    {
        QnCameraAdvancedParameter parameter = m_advancedParametersProvider.getParameterById(value.id);
        if (parameter.isValid())
        {
            if (kCproParameters.contains(parameter.id))
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

nx::vms::server::resource::StreamCapabilityMap
    QnDigitalWatchdogResource::getStreamCapabilityMapFromDriver(MotionStreamType streamIndex)
{
    NX_VERBOSE(this, "%1(%2): id %3", __func__, streamIndex, getId());

    auto onvifResult = base_type::getStreamCapabilityMapFromDriver(streamIndex);

    if (!this->getMedia2Url().isEmpty())
    {
        // Media2 webService allows to detect H265 encoders, so we don't need to use
        // additional dw-specific detection.
        return onvifResult;
    }
    const auto codecs = m_cproApiClient->getSupportedVideoCodecs(streamIndex);
    if (codecs)
    {
        nx::vms::server::resource::StreamCapabilityMap result;
        for (const auto& codec: *codecs)
        {
            for (const auto& onvifKeys: onvifResult.keys())
            {
                nx::vms::server::resource::StreamCapabilityKey key;
                key.codec = codec.toUpper();
                key.resolution = onvifKeys.resolution;
                result.insert(key, nx::media::CameraStreamCapability());
            }
        }
        return result;
    }

    const auto resourceUrl = nx::utils::Url(getUrl());
    JsonApiClient jsonClient({resourceUrl.host(), static_cast<quint16>(resourceUrl.port())}, getAuth());
    const auto codecsFromJson = jsonClient.getSupportedVideoCodecs(getChannel(), streamIndex);
    if (codecsFromJson.empty())
        return onvifResult;

    m_isJsonApiSupported = true;
    return codecsFromJson;

}

CameraDiagnostics::Result QnDigitalWatchdogResource::sendVideoEncoderToCameraEx(
    VideoEncoder& encoder,
    MotionStreamType streamIndex,
    const QnLiveStreamParams& streamParams)
{
    NX_VERBOSE(this, "%1(%2): id %3", __func__, streamIndex, getId());
    if (streamParams.codec == "H265" && m_isJsonApiSupported)
    {
        const auto resourceUrl = nx::utils::Url(getUrl());
        JsonApiClient jsonClient({resourceUrl.host(), static_cast<quint16>(resourceUrl.port())}, getAuth());
        if (!jsonClient.sendStreamParams(getChannel(), streamIndex, streamParams))
            return CameraDiagnostics::CannotConfigureMediaStreamResult("Codec");

        return CameraDiagnostics::NoErrorResult();
    }

    auto result = base_type::sendVideoEncoderToCameraEx(encoder, streamIndex, streamParams);
    if (result)
        return result; //< Onvif videoencoder update was successful.

    if (!m_cproApiClient->setVideoCodec(streamIndex, streamParams.codec))
        NX_WARNING(this, lm("Failed to configure codec %1 for resource %2").args(streamParams.codec, getUrl()));
    return CameraDiagnostics::NoErrorResult();
}

#endif //ENABLE_ONVIF
