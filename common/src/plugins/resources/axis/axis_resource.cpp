#include "axis_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "axis_stream_reader.h"

const char* QnPlAxisResource::MANUFACTURE = "Axis";


QnPlAxisResource::QnPlAxisResource()
{
    setAuth("root", "1");
}

bool QnPlAxisResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlAxisResource::updateMACAddress()
{
    return true;
}

QString QnPlAxisResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlAxisResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlAxisResource::createLiveDataProvider()
{
    //return new MJPEGtreamreader(toSharedPointer(), "mjpg/video.mjpg");
    return new QnAxisStreamReader(toSharedPointer());
}

void QnPlAxisResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnPlAxisResource::isInitialized() const
{
    return !m_maxResolution.isEmpty();
}

void QnPlAxisResource::clear()
{
    m_maxResolution.clear();
}

void QnPlAxisResource::init()
{
    m_maxResolution.clear();
    // determin camera max resolution
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Properties.Image.Resolution"));
    if (status != CL_HTTP_SUCCESS)
        return;
        
    QByteArray body;
    http.readAll(body);
    int paramValuePos = body.indexOf('=');
    if (paramValuePos == -1)
    {
        qWarning() << Q_FUNC_INFO << "Unexpected server answer. Can't read resolution list";
        return;
    }

    QList<QByteArray> resolutionList = body.mid(paramValuePos+1).split(',');
    m_maxResolution = resolutionList[0]; 
}

QString QnPlAxisResource::getMaxResolution() const
{
    return m_maxResolution;
}
