#include "digital_watchdog_resource.h"
#include "onvif/soapDeviceBindingProxy.h"

const QString CAMERA_SETTINGS_ID_PARAM = QString::fromLatin1("cameraSettingsId");

QString getIdSuffixByModel(const QString& cameraModel)
{
    QString tmp = cameraModel.toLower();
    tmp = tmp.replace(QLatin1String(" "), QLatin1String(""));

    if (tmp.contains(QLatin1String("mv421d")) || tmp.contains(QLatin1String("md421d"))) {
        return QString::fromLatin1("-FOCUS");
    }

    return QString();
}

QnPlWatchDogResource::QnPlWatchDogResource():
    QnPlOnvifResource(),
    m_additionalSettings()
{

}

QnPlWatchDogResource::~QnPlWatchDogResource()
{

}

bool QnPlWatchDogResource::isDualStreamingEnabled()
{
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
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
        setStatus(Unauthorized);
    }

    return false;
}

bool QnPlWatchDogResource::initInternal() 
{
    if (!isDualStreamingEnabled() && getStatus() != QnResource::Unauthorized) 
    {
        // The camera most likely is going to reset after enabling dual streaming
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
        QByteArray request;
        request.append("onvif_stream_number=2&onvif_use_service=true&onvif_service_port=8032&");
        request.append("onvif_use_discovery=true&onvif_use_security=true&onvif_security_opts=63&onvif_use_sa=true&reboot=true");
        CLHttpStatus status = http.doPOST(QByteArray("/cgi-bin/onvifsetup.cgi"), QLatin1String(request));
        Q_UNUSED(status);

        setStatus(Offline);
        return false;
    }
    else 
    {
        return QnPlOnvifResource::initInternal();
    }
}


int QnPlWatchDogResource::suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const
{
    // I assume for a QnQualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a QnQualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*11;
    int lowEnd = 1024*1.8;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(resolutionFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - QnQualityLowest) / (QnQualityHighest - QnQualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(512,result);
}

void QnPlWatchDogResource::fetchAndSetCameraSettings()
{
    QnPlOnvifResource::fetchAndSetCameraSettings();

    QString cameraModel = fetchCameraModel();
    QVariant baseId;
    getParam(CAMERA_SETTINGS_ID_PARAM, baseId, QnDomainDatabase);
    QString baseIdStr = baseId.toString();

    QString suffix = getIdSuffixByModel(cameraModel);
    if (!suffix.isEmpty()) {
        if (baseIdStr.endsWith(suffix))
        {
            baseIdStr = baseIdStr.left(baseIdStr.length() - suffix.length());
        }
        else
        {
            setParam(CAMERA_SETTINGS_ID_PARAM, baseIdStr + suffix, QnDomainDatabase);
        }
    }

    QMutexLocker lock(&m_physicalParamsMutex);

    m_additionalSettings.push_front(QnPlWatchDogResourceAdditionalSettingsPtr(new QnPlWatchDogResourceAdditionalSettings(
        getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth(), baseIdStr)));

    if (!suffix.isEmpty()) {
        m_additionalSettings.push_front(QnPlWatchDogResourceAdditionalSettingsPtr(new QnPlWatchDogResourceAdditionalSettings(
            getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth(), baseIdStr + suffix)));
    }
}

QString QnPlWatchDogResource::fetchCameraModel()
{
    QAuthenticator auth(getAuth());
    //TODO:UTF unuse StdString
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user().toStdString(), auth.password().toStdString());

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

    QDateTime currTime = QDateTime::currentDateTime().addSecs(-ADVANCED_SETTINGS_VALID_TIME);
    if (currTime > m_advSettingsLastUpdated) {
        foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
        {
            if (!setting->refreshValsFromCamera())
            {
                return false;
            }
        }
        m_advSettingsLastUpdated = QDateTime::currentDateTime();
    }

    foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
    {
        if (setting->getParamPhysicalFromBuffer(param, val))
        {
            return true;
        }
    }

    return QnPlOnvifResource::getParamPhysical(param, val);
}

bool QnPlWatchDogResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    QMutexLocker lock(&m_physicalParamsMutex);

    foreach (QnPlWatchDogResourceAdditionalSettingsPtr setting, m_additionalSettings)
    {
        if (setting->setParamPhysical(param, val))
        {
            return true;
        }
    }

    return QnPlOnvifResource::setParamPhysical(param, val);
}

//
// class QnPlWatchDogResourceAdditionalSettings
//

QnPlWatchDogResourceAdditionalSettings::QnPlWatchDogResourceAdditionalSettings(const QHostAddress& host,
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
