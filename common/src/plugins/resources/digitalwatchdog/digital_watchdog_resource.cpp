#include "digital_watchdog_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "dw_zoom_ptz_controller.h"
#include "utils/math/space_mapper.h"

const QString CAMERA_SETTINGS_ID_PARAM = QString::fromLatin1("cameraSettingsId");
static const int HTTP_PORT = 80;

QString getIdSuffixByModel(const QString& cameraModel)
{
    QString tmp = cameraModel.toLower();
    tmp = tmp.replace(QLatin1String(" "), QLatin1String(""));

    if (tmp.contains(QLatin1String("mpa20m")) || tmp.contains(QLatin1String("mc421"))) {
        //without focus
        return QString::fromLatin1("-WOFOCUS");
    }

    return QString::fromLatin1("-FOCUS");
}

QnPlWatchDogResource::QnPlWatchDogResource():
    QnPlOnvifResource(),
    m_additionalSettings()
{

}

QnPlWatchDogResource::~QnPlWatchDogResource()
{

}

bool QnPlWatchDogResource::isDualStreamingEnabled(bool& unauth)
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
        setStatus(Unauthorized);
        return false;
    }
    
    return true; // ignore other error (for cameras with non standart HTTP port)
}

bool QnPlWatchDogResource::initInternal() 
{
    bool unauth = false;
    if (!isDualStreamingEnabled(unauth) && unauth==false) 
    {
        if (m_appStopping)
            return false;

        // The camera most likely is going to reset after enabling dual streaming
        enableOnvifSecondStream();
        return false;
    }
        
    bool result = QnPlOnvifResource::initInternal();

    // TODO: #Elric this code is totally evil. Better write it properly as soon as possible.
    CLSimpleHTTPClient http(getHostAddress(), HTTP_PORT, getNetworkTimeout(), getAuth());
    http.doGET(QByteArray("/cgi-bin/getconfig.cgi?action=color"));
    QByteArray data;
    http.readAll(data);

    bool flipVertical = false, flipHorizontal = false;
    if(data.contains("flipmode1: 1")) {
        flipHorizontal = !flipHorizontal;
        flipVertical = !flipVertical;
    }
    if(data.contains("mirrormode1: 1"))
        flipHorizontal = !flipHorizontal;

    // TODO: #Elric evil hacks here =(
    if(QnOnvifPtzController *ptzController = dynamic_cast<QnOnvifPtzController *>(base_type::getPtzController())) {
        ptzController->setFlipped(flipHorizontal, flipVertical);

        if(QnPtzSpaceMapper *mapper = const_cast<QnPtzSpaceMapper *>(ptzController->getSpaceMapper())) {
            QnVectorSpaceMapper &fromCamera = const_cast<QnVectorSpaceMapper &>(mapper->fromCamera());
            QnVectorSpaceMapper &toCamera = const_cast<QnVectorSpaceMapper &>(mapper->toCamera());
            if(flipHorizontal) {
                fromCamera.setMapper(QnVectorSpaceMapper::X, fromCamera.mapper(QnVectorSpaceMapper::X).flipped(false, true, 0.0, 0.0));
                toCamera.setMapper(QnVectorSpaceMapper::X, toCamera.mapper(QnVectorSpaceMapper::X).flipped(false, true, 0.0, 0.0));
            }
            if(flipVertical) {
                fromCamera.setMapper(QnVectorSpaceMapper::Y, fromCamera.mapper(QnVectorSpaceMapper::Y).flipped(false, true, 0.0, 0.0));
                toCamera.setMapper(QnVectorSpaceMapper::Y, toCamera.mapper(QnVectorSpaceMapper::Y).flipped(false, true, 0.0, 0.0));
            }
        }
    }

    return result;
}


void QnPlWatchDogResource::enableOnvifSecondStream()
{
    // The camera most likely is going to reset after enabling dual streaming
    CLSimpleHTTPClient http (getHostAddress(), HTTP_PORT, getNetworkTimeout(), getAuth());
    QByteArray request;
    request.append("onvif_stream_number=2&onvif_use_service=true&onvif_service_port=8032&");
    request.append("onvif_use_discovery=true&onvif_use_security=true&onvif_security_opts=63&onvif_use_sa=true&reboot=true");
    http.doPOST(QByteArray("/cgi-bin/onvifsetup.cgi"), QLatin1String(request));

    setStatus(Offline);
    // camera rebooting ....
}

int QnPlWatchDogResource::suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const
{
    // I assume for a QnQualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a QnQualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*9;
    int lowEnd = 1024*1.8;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(1024,result);
}

QnAbstractPtzController *QnPlWatchDogResource::getPtzController() {
    QnAbstractPtzController *result = base_type::getPtzController();
    if(result)
        return result; /* Use PTZ controller from ONVIF if one is present. */
    return m_ptzController.data();
}

void QnPlWatchDogResource::fetchAndSetCameraSettings()
{
    //The grandparent "ONVIF" is processed by invoking of parent 'fetchAndSetCameraSettings' method
    QnPlOnvifResource::fetchAndSetCameraSettings();

    QString cameraModel = fetchCameraModel();
    QVariant baseId;
    getParam(CAMERA_SETTINGS_ID_PARAM, baseId, QnDomainDatabase);
    QString baseIdStr = baseId.toString();

    QString suffix = getIdSuffixByModel(cameraModel);
    if (!suffix.isEmpty()) {
        bool hasFocus = suffix.endsWith(QLatin1String("-FOCUS"));
        if(hasFocus) {
            m_ptzController.reset(new QnDwZoomPtzController(this));
            setPtzCapabilities(m_ptzController->getCapabilities());
        }

        QString prefix = baseIdStr.split(QLatin1String("-"))[0];
        QString fullCameraType = prefix + suffix;
        if (fullCameraType != baseIdStr)
            setParam(CAMERA_SETTINGS_ID_PARAM, fullCameraType, QnDomainDatabase);
        baseIdStr = prefix;
    }

    QMutexLocker lock(&m_physicalParamsMutex);

    //Put base model in list
    m_additionalSettings.push_front(QnPlWatchDogResourceAdditionalSettingsPtr(new QnPlWatchDogResourceAdditionalSettings(
        getHostAddress(), 80, getNetworkTimeout(), getAuth(), baseIdStr)));

    //Put expanded model in list
    if (!suffix.isEmpty()) {
        m_additionalSettings.push_front(QnPlWatchDogResourceAdditionalSettingsPtr(new QnPlWatchDogResourceAdditionalSettings(
            getHostAddress(), 80, getNetworkTimeout(), getAuth(), baseIdStr + suffix)));
    }
}

QString QnPlWatchDogResource::fetchCameraModel()
{
    QAuthenticator auth(getAuth());
    //TODO: #vasilenko UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString(), getTimeDrift());

    DeviceInfoReq request;
    DeviceInfoResp response;

    int soapRes = soapWrapper.getDeviceInformation(request, response);
    if (soapRes != SOAP_OK) 
    {
        qWarning() << "QnPlWatchDogResource::fetchCameraModel: GetDeviceInformation SOAP to endpoint "
            << soapWrapper.getEndpointUrl() << " failed. Camera name will remain 'Unknown'. GSoap error code: " << soapRes
            << ". " << soapWrapper.getLastError() << ". Only base (base for DW) advanced settings will be available for this camera.";

        return QString();
    } 

    return QString::fromLatin1(response.Model.c_str());
}

bool QnPlWatchDogResource::getParamPhysical(const QnParam &param, QVariant &val)
{
    QMutexLocker lock(&m_physicalParamsMutex);

    //Caching camera values during ADVANCED_SETTINGS_VALID_TIME to avoid multiple excessive 'get' requests 
    //to camera. All values can be get by one request, but our framework do getParamPhysical for every single param.
    QDateTime currTime = QDateTime::currentDateTime();
    if (m_advSettingsLastUpdated.isNull() || m_advSettingsLastUpdated.secsTo(currTime) > ADVANCED_SETTINGS_VALID_TIME) {
        foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
        {
            if (!setting->refreshValsFromCamera())
            {
                return false;
            }
        }
        m_advSettingsLastUpdated = currTime;
    }

    foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
    {
        //If param is not in list of child, it will return false. Then will try to find it in parent.
        if (setting->getParamPhysicalFromBuffer(param, val))
        {
            return true;
        }
    }

    //If param is not found in DW models, will try to find it in "ONVIF".
    return QnPlOnvifResource::getParamPhysical(param, val);
}

bool QnPlWatchDogResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    QMutexLocker lock(&m_physicalParamsMutex);

    foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
    {
        //If param is not in list of child, it will return false. Then will try to find it in parent.

        if (m_appStopping)
            return false;

        if (setting->setParamPhysical(param, val))
        {
            return true;
        }
    }

    //If param is not found in DW models, will try to find it in "ONVIF".
    return QnPlOnvifResource::setParamPhysical(param, val);
}

//
// class QnPlWatchDogResourceAdditionalSettings
//

QnPlWatchDogResourceAdditionalSettings::QnPlWatchDogResourceAdditionalSettings(const QString& host,
        int port, unsigned int timeout, const QAuthenticator& auth, const QString& cameraSettingId) :
    m_cameraProxy(new DWCameraProxy(host, port, timeout, auth)),
    m_settings()
{
    DWCameraSettingReader reader(m_settings, cameraSettingId);
    reader.read() && reader.proceed();
}

QnPlWatchDogResourceAdditionalSettings::~QnPlWatchDogResourceAdditionalSettings()
{
    delete m_cameraProxy;
}

bool QnPlWatchDogResourceAdditionalSettings::refreshValsFromCamera()
{
    return m_cameraProxy->getFromCameraIntoBuffer();
}

bool QnPlWatchDogResourceAdditionalSettings::getParamPhysicalFromBuffer(const QnParam &param, QVariant &val)
{
    DWCameraSettings::Iterator it = m_settings.find(param.name());
    if (it != m_settings.end()) {
        if (it.value().getFromBuffer(*m_cameraProxy)){
            val.setValue(it.value().serializeToStr());
            return true;
        }

        //If server can't get value from camera, it will be marked in "QVariant &val" as empty m_current param
        //Completely empty "QVariant &val" means enabled setting with no value (ex: Settings tree element or button)
        //Can't return false in this case, because our framework stops fetching physical params after first failed.
        return true;
    }

    return false;
}

bool QnPlWatchDogResourceAdditionalSettings::setParamPhysical(const QnParam &param, const QVariant& val)
{
    CameraSetting tmp;
    tmp.deserializeFromStr(val.toString());

    DWCameraSettings::Iterator it = m_settings.find(param.name());
    if (it != m_settings.end()) {
        CameraSettingValue oldVal = it.value().getCurrent();
        it.value().setCurrent(tmp.getCurrent());

        if (!it.value().setToCamera(*m_cameraProxy)) {
            it.value().setCurrent(oldVal);
            return false;
        }

        return true;
    }

    return false;
}
