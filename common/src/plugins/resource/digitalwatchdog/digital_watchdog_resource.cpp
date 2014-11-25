#ifdef ENABLE_ONVIF

#include "digital_watchdog_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "dw_ptz_controller.h"
#include "dw_zoom_ptz_controller.h"
#include "common/common_module.h"
#include "core/resource_management/resource_data_pool.h"
#include "newdw_ptz_controller.h"

static const int HTTP_PORT = 80;

QString getIdSuffixByModel(const QString& cameraModel)
{
    QString tmp = cameraModel.toLower();
    tmp = tmp.replace(QLatin1String(" "), QLatin1String(""));

    if (tmp.contains(QLatin1String("fd20")) || tmp.contains(QLatin1String("mpa20m")) || tmp.contains(QLatin1String("mc421"))) {
        //without focus
        return lit("-WOFOCUS");
    }

    return lit("-FOCUS");
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

void QnDigitalWatchdogResource::fetchAndSetCameraSettings()
{
    //The grandparent "ONVIF" is processed by invoking of parent 'fetchAndSetCameraSettings' method
    QnPlOnvifResource::fetchAndSetCameraSettings();

    QString cameraModel = fetchCameraModel();
    QString baseIdStr = getProperty(Qn::CAMERA_SETTINGS_ID_PARAM_NAME);

    QString suffix = getIdSuffixByModel(cameraModel);
    if (!suffix.isEmpty()) {
        if(suffix.endsWith(QLatin1String("-FOCUS")))
            m_hasZoom = true;

        QString prefix = baseIdStr.split(QLatin1String("-"))[0];
        QString fullCameraType = prefix + suffix;
        if (fullCameraType != baseIdStr)
            setProperty(Qn::CAMERA_SETTINGS_ID_PARAM_NAME, fullCameraType);
        baseIdStr = prefix;
    }

    QMutexLocker lock(&m_physicalParamsMutex);
    //TODO: #GDM global object
    m_advancedParameters = QnCameraAdvancedParamsReader().params(this->toSharedPointer());

    m_cameraProxy.reset(new DWCameraProxy(getHostAddress(), 80, getNetworkTimeout(), getAuth()));
    loadPhysicalParams();
}

QString QnDigitalWatchdogResource::fetchCameraModel()
{
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


bool QnDigitalWatchdogResource::loadPhysicalParams() {
    if (!m_cameraProxy)
        return false;
    m_advancedParamsCache.clear();
    m_advancedParamsCache.appendValueList(m_cameraProxy->getParamsList());
    return true;
}

bool QnDigitalWatchdogResource::getParamPhysical(const QString &id, QString &value) {
    QMutexLocker lock(&m_physicalParamsMutex);

    //Caching camera values during ADVANCED_SETTINGS_VALID_TIME to avoid multiple excessive 'get' requests 
    //to camera. All values can be get by one request, but our framework do getParamPhysical for every single param.
    QDateTime curTime = QDateTime::currentDateTime();
    if (m_advSettingsLastUpdated.isNull() || m_advSettingsLastUpdated.secsTo(curTime) > ADVANCED_SETTINGS_VALID_TIME) {
        if (loadPhysicalParams())
            m_advSettingsLastUpdated = curTime;
    }

    if (!m_advancedParamsCache.contains(id))
        return false;
    value = m_advancedParamsCache[id];
    return true;
}

bool QnDigitalWatchdogResource::setParamPhysical(const QString &id, const QString &value) {
    if (m_appStopping)
        return false;

    bool result = false;
    {
        QMutexLocker lock(&m_physicalParamsMutex);
        if (!m_cameraProxy)
            return false;

        QnCameraAdvancedParameter parameter = m_advancedParameters.getParameterById(id);
        Q_ASSERT_X(parameter.isValid(), Q_FUNC_INFO, "parameter id should be correct here by design");
        if (!parameter.isValid())
            return false;

        m_advancedParamsCache[id] = value;
        result = m_cameraProxy->setParam(id, value, parameter.method);
    }
    emit physicalParamChanged(id, value);
    return result;
}

#endif //ENABLE_ONVIF
