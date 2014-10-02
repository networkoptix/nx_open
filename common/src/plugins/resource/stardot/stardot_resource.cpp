#ifdef ENABLE_STARDOT

#include "stardot_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "stardot_stream_reader.h"
#include <business/business_event_rule.h>
#include "utils/common/synctime.h"


static QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
const QString QnStardotResource::MANUFACTURE(lit("Stardot"));
static const int TCP_TIMEOUT = 3000;
static const int DEFAULT_RTSP_PORT = 554;

namespace 
{
    int resSquare(const QSize& size)
    {
        return size.width() * size.height();
    }
    QString formatCellStr(int val) {
        return QString::number(val, 16).toUpper();
    }

};

QnStardotResource::QnStardotResource():
    m_hasAudio(false),
    m_resolutionNum(-1),
    m_maxFps(30),
    m_rtspPort(DEFAULT_RTSP_PORT),
    m_rtspTransport(lit("tcp")),
    m_motionMaskBinData(0)
{
    setVendor(lit("Stardot"));
    setAuth(lit("admin"), lit("admin"));
}

QnStardotResource::~QnStardotResource()
{
    qFreeAligned(m_motionMaskBinData);
}

QString QnStardotResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnStardotResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

QnAbstractStreamDataProvider* QnStardotResource::createLiveDataProvider()
{
    return new QnStardotStreamReader(toSharedPointer());
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

QByteArray QnStardotResource::makeStardotPostRequest(const QString& request, const QString& body, CLHttpStatus& status) const
{
    QByteArray result;

    QUrl url(getUrl());
    CLSimpleHTTPClient client(getHostAddress(), 80, TCP_TIMEOUT, getAuth());
    status = client.doPOST(request, body);
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
                m_rtspTransport = QLatin1String(data[1]);
        }
    }
}

CameraDiagnostics::Result QnStardotResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    CLHttpStatus status;
       
    QByteArray resList = makeStardotRequest(lit("info.cgi?resolutions&api=2"), status);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();
    detectMaxResolutionAndFps(resList);
    if (m_resolution.isEmpty() || m_maxFps < 1)
        return CameraDiagnostics::UnknownErrorResult();


    QByteArray info = makeStardotRequest(lit("info.cgi"), status);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();
    parseInfo(info);
    if (m_rtspPort == 0) 
    {
        qWarning() << "No H.264 RTSP port found for Stardot camera" << getHostAddress() << "Please update camera firmware";
        return CameraDiagnostics::UnknownErrorResult();
    }

    //setParam(AUDIO_SUPPORTED_PARAM_NAME, m_hasAudio ? 1 : 0, QnDomainDatabase);
    setParam(MAX_FPS_PARAM_NAME, m_maxFps, QnDomainDatabase);
    //setParam(DUAL_STREAMING_PARAM_NAME, !m_resolution[1].isEmpty() ? 1 : 0, QnDomainDatabase);

    //detecting and saving selected resolutions
    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolution, CODEC_ID_H264 ) ); //QnStardotStreamReader always requests h.264
    saveResolutionList( mediaStreams );

    saveParams();

    setMotionMaskPhysical(0);
    return CameraDiagnostics::NoErrorResult();
}

bool QnStardotResource::isResourceAccessible()
{
    return updateMACAddress();
}

QnConstResourceAudioLayoutPtr QnStardotResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
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
    url.setPath(lit("/nph-h264.cgi/"));
    return url.toString();
}

int QnStardotResource::getMaxFps() const
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

    QnMotionRegion region = getMotionRegion(0);
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {

        if (!region.getRegionBySens(sens).isEmpty())
        {
            CLHttpStatus status;
            QString body(lit("motion_noise=%1&motion_sensitivity=%2&motion_time=4"));
            body = body.arg(16-sens).arg(sens);
            for (int y = 0; y < 16; ++y)
            {
                for (int x = 0; x < 16; ++x) {
                    body += QString(lit("&motion_grid_%1%2=on")).arg(formatCellStr(y)).arg(formatCellStr(x));
                }
            }

            makeStardotPostRequest(lit("admin.cgi?motion"), body, status);
            break; // only 1 sensitivity for all frame is supported
        }
    }
    if (m_motionMaskBinData == 0)
        m_motionMaskBinData = (simd128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
    QnMetaDataV1::createMask(getMotionMask(0), (char*)m_motionMaskBinData);
}

simd128i* QnStardotResource::getMotionMaskBinData() const
{
    return m_motionMaskBinData;
}

#endif // #ifdef ENABLE_STARDOT
