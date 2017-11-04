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

static const int HTTP_PORT = 80;

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
    m_hasZoom(false)
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
        return base_type::loadAdvancedParametersTemplate(params); //< dw-cpro chipset (or something else that has uncompatible cgi interface)
    else if (resourceData.value<bool>(lit("dw-pravis-chipset")))
        return loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw-pravis.xml"));
    else
        return loadXmlParametersInternal(params, lit(":/camera_advanced_params/dw.xml"));
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
    if (useOnvifAdvancedParameterProviders())
        return base_type::calculateSupportedAdvancedParameters();

    QSet<QString> result = base_type::calculateSupportedAdvancedParameters();
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


bool QnDigitalWatchdogResource::loadAdvancedParamsUnderLock(QnCameraAdvancedParamValueMap &values) {
    bool baseResult = base_type::loadAdvancedParamsUnderLock(values);

    if (!m_cameraProxy)
        return baseResult;
    values.appendValueList(m_cameraProxy->getParamsList());
    return true;
}

bool QnDigitalWatchdogResource::setAdvancedParameterUnderLock(const QnCameraAdvancedParameter &parameter, const QString &value) {
    bool baseResult = base_type::setAdvancedParameterUnderLock(parameter, value);
    if (baseResult)
        return true;
    if (!m_cameraProxy)
        return false;

    QVector<QPair<QnCameraAdvancedParameter, QString>> params;
    params << QPair<QnCameraAdvancedParameter, QString>(parameter, value);

    return m_cameraProxy->setParams(params);
}

bool QnDigitalWatchdogResource::setAdvancedParametersUnderLock(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    if (useOnvifAdvancedParameterProviders())
        return base_type::setAdvancedParametersUnderLock(values, result);

    bool success = true;
    QVector<QPair<QnCameraAdvancedParameter, QString>> moreParamsToProcess;
    for(const QnCameraAdvancedParamValue &value: values)
    {
        QnCameraAdvancedParameter parameter = m_advancedParameters.getParameterById(value.id);
        if (parameter.isValid()) {
            bool baseResult = base_type::setAdvancedParameterUnderLock(parameter, value.value);
            if (baseResult)
                result << value;
            else
                moreParamsToProcess << QPair<QnCameraAdvancedParameter, QString>(parameter, value.value);
        }
        else
            success = false;
    }
    if (!success)
        return false;
    return m_cameraProxy->setParams(moreParamsToProcess, &result);
}

#endif //ENABLE_ONVIF
