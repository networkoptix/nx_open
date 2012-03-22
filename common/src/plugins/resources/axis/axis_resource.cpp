#include "axis_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "axis_stream_reader.h"

const char* QnPlAxisResource::MANUFACTURE = "Axis";
static const int MAX_AR_EPS = 0.01;

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
    return !m_resolutionList.isEmpty();
}

void QnPlAxisResource::clear()
{
    m_resolutionList.clear();
}

void QnPlAxisResource::init()
{
    QMutexLocker lock(&m_mutex);
    m_resolutionList.clear();
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

    m_resolutionList = body.mid(paramValuePos+1).split(',');
    for (int i = 0; i < m_resolutionList.size(); ++i)
        m_resolutionList[i] = m_resolutionList[i].toLower().trimmed();
}

QByteArray QnPlAxisResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return !m_resolutionList.isEmpty() ? m_resolutionList[0] : QByteArray();
}

float QnPlAxisResource::getResolutionAspectRatio(const QByteArray& resolution) const
{
    QList<QByteArray> dimensions = resolution.split('x');
    if (dimensions.size() != 2)
    {
        qWarning() << Q_FUNC_INFO << "invalid resolution format. Expected widthxheight";
        return 1.0;
    }
    return dimensions[0].toFloat()/dimensions[1].toFloat();
}


QString QnPlAxisResource::getNearestResolution(const QByteArray& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);
    QList<QByteArray> dimensions = resolution.split('x');
    if (dimensions.size() != 2)
    {
        qWarning() << Q_FUNC_INFO << "invalid request resolution format. Expected widthxheight";
        return QString();
    }
    float requestSquare = dimensions[0].toInt() * dimensions[1].toInt();
    int bestIndex = -1;
    float bestMatchCoeff = INT_MAX;
    for (int i = 0; i < m_resolutionList.size(); ++ i)
    {
        float ar = getResolutionAspectRatio(m_resolutionList[i]);
        if (qAbs(ar-aspectRatio) > MAX_AR_EPS)
            continue;
        dimensions = m_resolutionList[i].split('x');
        if (dimensions.size() != 2)
            continue;
        float square = dimensions[0].toInt() * dimensions[1].toInt();
        float matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff < bestMatchCoeff)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }
    return bestIndex >= 0 ? m_resolutionList[bestIndex] : resolution;
}
