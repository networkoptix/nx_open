#ifdef ENABLE_ONVIF

#include "newdw_ptz_controller.h"
#include "plugins/resource/digitalwatchdog/digital_watchdog_resource.h"
#include "utils/math/fuzzy.h"

QnNewDWPtzController::QnNewDWPtzController(const QnPlWatchDogResourcePtr &resource):
    QnBasicPtzController(resource),
    m_resource(resource)
{
}

QnNewDWPtzController::~QnNewDWPtzController()
{

}

Qn::PtzCapabilities QnNewDWPtzController::getCapabilities()
{
    return Qn::ContinuousZoomCapability | Qn::ContinuousPanCapability;
}

static QString zoomDirection(qreal speed) 
{
    if(qFuzzyIsNull(speed))
        return lit("stop");
    else if(speed < 0) 
        return lit("out");
    else
        return lit("in");
}

static int toNativeSpeed(qreal speed)
{
    return int (qAbs(speed * 10) + 0.5);
}

static QString panDirection(qreal speed) 
{
    if(qFuzzyIsNull(speed))
        return lit("stop");
    else if(speed < 0) 
        return lit("left");
    else
        return lit("right");
}

bool QnNewDWPtzController::continuousMove(const QVector3D &speed)
{
    QString request;
    if (!qFuzzyIsNull(speed.z()))
        request = QString(lit("cgi-bin/ptz.cgi?zoom=%1")).arg(zoomDirection(speed.z()));
    else
        request = QString(lit("/cgi-bin/ptz.cgi?speed=%1&move=%2")).arg(toNativeSpeed(speed.x())).arg(panDirection(speed.x()));
    return doQuery(request);
}

bool QnNewDWPtzController::doQuery(const QString &request) const 
{
    static const int TCP_TIMEOUT = 1000 * 5;

    QUrl url(m_resource->getUrl());
    url.setPath(request);

    QAuthenticator auth = m_resource->getAuth();
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, m_resource->getAuth());
    CLHttpStatus status = client.doGET(request);
    return status == CL_HTTP_SUCCESS;
}

#endif // ENABLE_ONVIF
