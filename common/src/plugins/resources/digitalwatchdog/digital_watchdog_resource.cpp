#include "digital_watchdog_resource.h"

QnPlWatchDogResource::QnPlWatchDogResource()
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
        QList<QByteArray> lines = body.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].toLower().startsWith("onvif_stream_number")) {
                QList<QByteArray> params = lines[i].split(':');
                if (params.size() >= 2) {
                    int streams = params[1].trimmed().toInt();
                    return streams >= 2;
                }
            }
        }
    }
    return false;
}

void QnPlWatchDogResource::initInternal() 
{
    if (!isDualStreamingEnabled()) {
        // The camera most likely is going to reset after enabling dual streaming
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
        QByteArray request;
        request.append("onvif_stream_number=2&onvif_use_service=true&onvif_service_port=8032&");
        request.append("onvif_use_discovery=true&onvif_use_security=true&onvif_security_opts=63&onvif_use_sa=true&reboot=true");
        CLHttpStatus status = http.doPOST(QByteArray("/cgi-bin/onvifsetup.cgi"), request);
        setStatus(Offline);
    }
    else {
        QnPhysicalCameraResource::initInternal();
    }
}
