#include "digital_watchdog_resource.h"

QnPlWatchDogResource::QnPlWatchDogResource():
    QnPlOnvifResource(),
    m_cameraProxy(0),
    m_settings()
{

}

QnPlWatchDogResource::~QnPlWatchDogResource()
{
    delete m_cameraProxy;
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
        bool res = QnPlOnvifResource::initInternal();
        if (res) {
            m_cameraProxy = new DWCameraProxy(getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
        }

        return res;
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
    if (!m_cameraProxy) {
        //ToDo: log warning
        return;
    }

    QnPlOnvifResource::fetchAndSetCameraSettings();
}

bool QnPlWatchDogResource::getParamPhysical(const QnParam &param, QVariant &val)
{
    if (!m_cameraProxy) {
        //ToDo: log warning
        return false;
    }

    CameraSettings::ConstIterator it = m_settings.find(param.name());
    if (it != m_settings.end()) {
        if (it.value().getFromCamera(*m_cameraProxy)){
            val.setValue(static_cast<QString>(it.value().getCurrent()));
            return true;
        }

        return false;
    }

    return QnPlOnvifResource::getParamPhysical(param, val);
}

bool QnPlWatchDogResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    if (!m_cameraProxy) {
        //ToDo: log warning
        return false;
    }

    CameraSettings::ConstIterator it = m_settings.find(param.name());
    if (it != m_settings.end()) {
        it.value().setCurrent(val.toString());
        return it.value().setToCamera(*m_cameraProxy));
    }

    return QnPlOnvifResource::setParamPhysical();
}
