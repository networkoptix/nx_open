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

QnPlWatchDogResource::QnPlWatchDogResource():
    m_hasZoom(false)
{
    setVendor(lit("Digital Watchdog"));
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
        setStatus(Qn::Unauthorized);
        return false;
    }
    
    return true; // ignore other error (for cameras with non standart HTTP port)
}

CameraDiagnostics::Result QnPlWatchDogResource::initInternal() 
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


void QnPlWatchDogResource::enableOnvifSecondStream()
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

int QnPlWatchDogResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    int realBitrate = QnPlOnvifResource::suggestBitrateKbps(q, resolution, fps);
    return realBitrate * (30.0 / (qreal)fps);
}

QnAbstractPtzController *QnPlWatchDogResource::createPtzControllerInternal() 
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

void QnPlWatchDogResource::fetchAndSetCameraSettings()
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

    m_additionalSettings.clear();

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
    DeviceSoapWrapper soapWrapper(getDeviceOnvifUrl().toStdString(), auth.user(), auth.password(), getTimeDrift());

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

    return QString::fromUtf8(response.Model.c_str());
}

bool QnPlWatchDogResource::getParamPhysical(const QString &param, QVariant &val)
{
    QMutexLocker lock(&m_physicalParamsMutex);

    //Caching camera values during ADVANCED_SETTINGS_VALID_TIME to avoid multiple excessive 'get' requests 
    //to camera. All values can be get by one request, but our framework do getParamPhysical for every single param.
    QDateTime currTime = QDateTime::currentDateTime();
    if (m_advSettingsLastUpdated.isNull() || m_advSettingsLastUpdated.secsTo(currTime) > ADVANCED_SETTINGS_VALID_TIME) {
        for (const QnPlWatchDogResourceAdditionalSettingsPtr& setting: m_additionalSettings)
        {
            if (!setting->refreshValsFromCamera())
            {
                return false;
            }
        }
        m_advSettingsLastUpdated = currTime;
    }

    for (const QnPlWatchDogResourceAdditionalSettingsPtr& setting: m_additionalSettings)
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

bool QnPlWatchDogResource::setParamPhysical(const QString &param, const QVariant& val )
{
    QMutexLocker lock(&m_physicalParamsMutex);

    for (const QnPlWatchDogResourceAdditionalSettingsPtr& setting: m_additionalSettings)
    {
        //If param is not in list of child, it will return false. Then will try to find it in parent.

        if (m_appStopping)
            return false;

        if (setting->setParamPhysical(param, val))
        {
            QString realParamName = param.split(lit("%%")).last();
            QString realParamValue = val.toString().split(lit(";")).last();
            emit physicalParamChanged(realParamName, realParamValue);

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
}

bool QnPlWatchDogResourceAdditionalSettings::refreshValsFromCamera()
{
    return m_cameraProxy->getFromCameraIntoBuffer();
}

bool QnPlWatchDogResourceAdditionalSettings::getParamPhysicalFromBuffer(const QString &param, QVariant &val)
{
    DWCameraSettings::Iterator it = m_settings.find(param);
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

bool QnPlWatchDogResourceAdditionalSettings::setParamPhysical(const QString &param, const QVariant& val)
{
    CameraSetting tmp;
    tmp.deserializeFromStr(val.toString());

    DWCameraSettings::Iterator it = m_settings.find(param);
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

#endif //ENABLE_ONVIF
