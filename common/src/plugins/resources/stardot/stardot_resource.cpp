
#include "stardot_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "stardot_stream_reader.h"
#include <business/business_event_connector.h>
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include "utils/common/synctime.h"


static QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
const char* QnStardotResource::MANUFACTURE = "Stardot";
static const int TCP_TIMEOUT = 3000;
static const int DEFAULT_RTSP_PORT = 554;

namespace 
{
    int resSquare(const QSize& size)
    {
        return size.width() * size.height();
    }
};

QnStardotResource::QnStardotResource():
    m_rtspPort(DEFAULT_RTSP_PORT),
    m_hasAudio(false),
    m_maxFps(30),
    m_resolutionNum(-1),
    m_rtspTransport(lit("tcp")),
    m_motionMaskBinData(0)
{
    setAuth(lit("admin"), lit("admin"));
}

QnStardotResource::~QnStardotResource()
{
    qFreeAligned(m_motionMaskBinData);
}

QString QnStardotResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnStardotResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

QnAbstractStreamDataProvider* QnStardotResource::createLiveDataProvider()
{
    return new QnStardotStreamReader(toSharedPointer());
}

bool QnStardotResource::shoudResolveConflicts() const 
{
    return false;
}

void QnStardotResource::setCropingPhysical(QRect /*croping*/)
{

}

QSize QnStardotResource::extractResolution(const QByteArray& resolutionStr) const
{
    QList<QByteArray> params = resolutionStr.split('x');
    if (params.size() < 2 || params[0].isEmpty() || params[1].isEmpty())
        return QSize();
    bool isDigit = params[0].at(0) >= '0' && params[0].at(0) <= '9';
    if (!isDigit)
        params[0] = params[0].mid(1);
    
    return QSize(params[0].trimmed().toInt(), params[1].trimmed().toInt());
}

QByteArray QnStardotResource::makeStardotRequest(const QString& request, CLHttpStatus& status) const
{
    QByteArray result;

    QUrl url(getUrl());
    CLSimpleHTTPClient client(getHostAddress(), 80, TCP_TIMEOUT, getAuth());
    status = client.doGET(request);
    if (status == CL_HTTP_SUCCESS)
        client.readAll(result);
    return result;
}

void QnStardotResource::detectMaxResolutionAndFps(const QByteArray& resList)
{
    m_resolutionNum = -1;
    m_resolution = QSize();
    m_maxFps = 0;
    foreach(const QByteArray& line, resList.split('\n'))
    {
        QList<QByteArray> fields = line.split(',');
        if (fields.size() < 4)
            continue;
        QSize resolution = extractResolution(fields[1]);
        if (resSquare(resolution) > resSquare(m_resolution))
        {
            m_resolution = resolution;
            m_maxFps = fields[2].toInt();
            m_resolutionNum = fields[0].toInt();
        }
    }
}

void QnStardotResource::parseInfo(const QByteArray& info)
{
    m_rtspPort = 0;
    foreach(const QByteArray& line, info.split('\n'))
    {
        if (line.startsWith("h264_rtsp=")) {
            QByteArray portInfo = line.split('=')[1];
            portInfo = portInfo.split(',')[0];
            QList<QByteArray> data = portInfo.split('/');
            m_rtspPort = data[0].toInt();
            if (data.size() > 1)
                m_rtspTransport = lit(data[1]);
        }
    }
}

bool QnStardotResource::initInternal()
{
    CLHttpStatus status;
       
    QByteArray resList = makeStardotRequest(lit("info.cgi?resolutions&api=2"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;
    detectMaxResolutionAndFps(resList);
    if (m_resolution.isEmpty() || m_maxFps < 1)
        return false;


    QByteArray info = makeStardotRequest(lit("info.cgi"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;
    parseInfo(info);
    if (m_rtspPort == 0) 
    {
        qWarning() << "No H.264 RTSP port found for Stardot camera" << getHostAddress() << "Please update camera firmware";
        return false;
    }

    //setParam(AUDIO_SUPPORTED_PARAM_NAME, m_hasAudio ? 1 : 0, QnDomainDatabase);
    setParam(MAX_FPS_PARAM_NAME, m_maxFps, QnDomainDatabase);
    //setParam(DUAL_STREAMING_PARAM_NAME, !m_resolution[1].isEmpty() ? 1 : 0, QnDomainDatabase);
    save();

    setMotionMaskPhysical(0);
    return true;
}

bool QnStardotResource::isResourceAccessible()
{
    return updateMACAddress();
}

const QnResourceAudioLayout* QnStardotResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnStardotStreamReader* stardotReader = dynamic_cast<const QnStardotStreamReader*>(dataProvider);
        if (stardotReader && stardotReader->getDPAudioLayout())
            return stardotReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

QString QnStardotResource::getRtspUrl() const
{
    QUrl url;
    url.setScheme(lit("rtsp"));
    url.setHost(getHostAddress());
    url.setPort(m_rtspPort);
    url.setPath(lit("nph-h264.cgi"));
    return url.toString();
}

int QnStardotResource::getMaxFps()
{
    return m_maxFps;
}

QSize QnStardotResource::getResolution() const
{
    return m_resolution;
}

bool QnStardotResource::isAudioSupported() const
{
    return m_hasAudio;
}

bool QnStardotResource::hasDualStreaming() const
{
    return false;
}

void QnStardotResource::setMotionMaskPhysical(int channel)
{
    if (channel != 0)
        return;

    QnMotionRegion region = m_motionMaskList[0];
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {

        if (!region.getRegionBySens(sens).isEmpty())
        {
            CLHttpStatus status;
            makeStardotRequest(QString(lit("admin.cgi?motion&motion_sensitivity=%1")).arg(sens), status); // sensitivity in range 1..15, 1 is min sens
            makeStardotRequest(QString(lit("admin.cgi?motion&motion_noise=%1")).arg(16 - sens), status);  // additional motion filter in range 1..15, 15 - maximum filtering
            break; // only 1 sensitivity for all frame is supported
        }
    }
    if (m_motionMaskBinData == 0)
        m_motionMaskBinData = (__m128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
    QnMetaDataV1::createMask(getMotionMask(0), (char*)m_motionMaskBinData);
}

__m128i* QnStardotResource::getMotionMaskBinData() const
{
    return m_motionMaskBinData;
}
