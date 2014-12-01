#ifdef ENABLE_ONVIF

#include "digital_watchdog_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "dw_ptz_controller.h"
#include "dw_zoom_ptz_controller.h"
#include "common/common_module.h"
#include "core/resource_management/resource_data_pool.h"
#include "newdw_ptz_controller.h"
#include <utils/xml/camera_advanced_param_reader.h>

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

int QnDigitalWatchdogResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*9;
    int lowEnd = 1024*1.8;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(1024,result);
}

QnAbstractPtzController *QnDigitalWatchdogResource::createPtzControllerInternal() 
{
    QnResourceData resourceData = qnCommon->dataPool()->data(toSharedPointer(this));
    bool useHttpPtz = resourceData.value<bool>(lit("dw-http-ptz"), false);
    QScopedPointer<QnAbstractPtzController> result;
    if (useHttpPtz)
    {
        result.reset(new QnNewDWPtzController(toSharedPointer(this)));
    }
    else {
        result.reset(new QnDwPtzController(toSharedPointer(this)));
        if(result->getCapabilities() == Qn::NoPtzCapabilities) {
            result.reset();
            if(m_hasZoom)
                result.reset(new QnDwZoomPtzController(toSharedPointer(this)));
        }
    }
    return result.take();
}

bool QnDigitalWatchdogResource::loadAdvancedParametersTemplate(QnCameraAdvancedParams &params) const {
    QFile paramsTemplateFile(lit(":/camera_advanced_params/dw.xml"));
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    return QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);
}

void QnDigitalWatchdogResource::initAdvancedParametersProviders(QnCameraAdvancedParams &params) {
    base_type::initAdvancedParametersProviders(params);
    m_cameraProxy.reset(new DWCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
}

QSet<QString> QnDigitalWatchdogResource::calculateSupportedAdvancedParameters() const {
    QSet<QString> result = base_type::calculateSupportedAdvancedParameters();
    for (const QnCameraAdvancedParamValue& value: m_cameraProxy->getParamsList())
        result.insert(value.id);
    return result;
}

void QnDigitalWatchdogResource::fetchAndSetAdvancedParameters() {
    base_type::fetchAndSetAdvancedParameters();
    QString cameraModel = fetchCameraModel();
    m_hasZoom = modelHasZoom(cameraModel);
}

QString QnDigitalWatchdogResource::fetchCameraModel() {
    QAuthenticator auth(getAuth());
    //TODO: #vasilenko UTF unuse StdString
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
    return m_cameraProxy->setParam(parameter, value);
}

#endif //ENABLE_ONVIF
