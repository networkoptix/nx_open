#include "axis_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "axis_stream_reader.h"
#include "utils/common/synctime.h"

const char* QnPlAxisResource::MANUFACTURE = "Axis";
static const int MAX_AR_EPS = 0.01;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;

QnPlAxisResource::QnPlAxisResource()
{
    setAuth("root", "1");
    m_lastMotionReadTime = 0;
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
    QMutexLocker lock(&m_mutex);
    return !m_resolutionList.isEmpty();
}

void QnPlAxisResource::clear()
{
    m_resolutionList.clear();
}

struct WindowInfo
{
    enum RectType {Unknown, Motion, MotionMask};
    WindowInfo(): left(-1), right(-1), top(-1), bottom(-1), rectType(Unknown) {};
    bool isValid() const 
    { 
        return left >= 0 && right >= 0 && top >= 0 && bottom >= 0 && rectType != Unknown;
    }
    QRect toRect() const
    {
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }

    int left;
    int right;
    int top;
    int bottom;
    RectType rectType;
};

QRect QnPlAxisResource::axisRectToGridRect(const QRect& axisRect)
{
    qreal xCoeff = 9999.0 / (MD_WIDTH-1);
    qreal yCoeff = 9999.0 / (MD_HEIGHT-1);
    QPoint topLeft(axisRect.left()/xCoeff + 0.5, axisRect.top()/yCoeff + 0.5);
    QPoint bottomRight(axisRect.right()/xCoeff + 0.5, axisRect.bottom()/yCoeff + 0.5);
    return QRect(topLeft, bottomRight);
}

void QnPlAxisResource::readMotionInfo()
{
    qint64 time = qnSyncTime->currentMSecsSinceEpoch();
    if (time - m_lastMotionReadTime < MOTION_INFO_UPDATE_INTERVAL)
        return;
    m_lastMotionReadTime = time;

    QMutexLocker lock(&m_mutex);
    // read motion windows coordinates
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Motion"));
    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            setStatus(QnResource::Unauthorized);
        return;
    }
    QByteArray body;
    http.readAll(body);
    QList<QByteArray> lines = body.split('\n');
    QMap<int,WindowInfo> windows;

    for (int i = 0; i < lines.size(); ++i)
    {
        QList<QByteArray> params = lines[i].split('.');
        if (params.size() < 2)
            continue;
        int windowNum = params[params.size()-2].mid(1).toInt();
        QList<QByteArray> values = params[params.size()-1].split('=');
        if (values.size() < 2)
            continue;
        if (values[0] == "Left")
            windows[windowNum].left = values[1].toInt();
        else if (values[0] == "Right")
            windows[windowNum].right = values[1].toInt();
        else if (values[0] == "Top")
            windows[windowNum].top = values[1].toInt();
        else if (values[0] == "Bottom")
            windows[windowNum].bottom = values[1].toInt();
        if (values[0] == "WindowType")
        {
            if (values[1] == "include")
                windows[windowNum].rectType = WindowInfo::Motion;
            else
                windows[windowNum].rectType = WindowInfo::MotionMask;
        }
    }
    for (QMap<int,WindowInfo>::const_iterator itr = windows.begin(); itr != windows.end(); ++itr)
    {
        if (itr.value().isValid())
        {
            if (itr.value().rectType == WindowInfo::Motion)
                m_motionWindows[itr.key()] = axisRectToGridRect(itr.value().toRect());
            else
                m_motionMask[itr.key()] = axisRectToGridRect(itr.value().toRect());
        }
    }
}

void QnPlAxisResource::init()
{
    QMutexLocker lock(&m_mutex);

    {
        // enable send motion into H.264 stream
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
        CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes")); // &Image.TriggerDataEnabled=yes
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes&Image.I1.MPEG.UserDataEnabled=yes&Image.I2.MPEG.UserDataEnabled=yes&Image.I3.MPEG.UserDataEnabled=yes"));
        if (status != CL_HTTP_SUCCESS) {
            if (status == CL_HTTP_AUTH_REQUIRED)
                setStatus(QnResource::Unauthorized);
            return;
        }
    }

    readMotionInfo();

    // determin camera max resolution
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(80), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Properties.Image.Resolution"));
    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            setStatus(QnResource::Unauthorized);
        return;
    }

    m_resolutionList.clear();
        
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



    //root.Image.MotionDetection=no
    //root.Image.I0.TriggerData.MotionDetectionEnabled=yes
    //root.Image.I1.TriggerData.MotionDetectionEnabled=yes
    //root.Properties.Motion.MaxNbrOfWindows=10
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

QRect QnPlAxisResource::getMotionWindow(int num) const
{
    QMutexLocker lock(&m_mutex);
    return m_motionWindows.value(num);
}
