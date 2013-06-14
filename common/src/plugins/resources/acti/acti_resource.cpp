
#include "acti_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "acti_stream_reader.h"
#include <business/business_event_connector.h>
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include "utils/common/synctime.h"
#include "acti_ptz_controller.h"


const char* QnActiResource::MANUFACTURE = "ACTI";
static const int TCP_TIMEOUT = 3000;
static const int DEFAULT_RTSP_PORT = 7070;

QString AUDIO_SUPPORTED_PARAM_NAME = QLatin1String("isAudioSupported");
QString DUAL_STREAMING_PARAM_NAME = QLatin1String("hasDualStreaming");
QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
static int DEFAULT_AVAIL_BITRATE_KBPS[] = { 28, 56, 128, 256, 384, 500, 750, 1000, 1200, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000 };

QnActiResource::QnActiResource():
    m_rtspPort(DEFAULT_RTSP_PORT),
    m_hasAudio(false)
{
    setAuth(QLatin1String("admin"), QLatin1String("123456"));
    for (uint i = 0; i < sizeof(DEFAULT_AVAIL_BITRATE_KBPS)/sizeof(int); ++i)
        m_availBitrate << DEFAULT_AVAIL_BITRATE_KBPS[i];
}

QnActiResource::~QnActiResource()
{
}

QString QnActiResource::getDriverName() const
{
    return QLatin1String(MANUFACTURE);
}

void QnActiResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnActiResource::createLiveDataProvider()
{
    return new QnActiStreamReader(toSharedPointer());
}

bool QnActiResource::shoudResolveConflicts() const 
{
    return false;
}

void QnActiResource::setCropingPhysical(QRect /*croping*/)
{

}

QSize QnActiResource::extractResolution(const QByteArray& resolutionStr) const
{
    QList<QByteArray> params = resolutionStr.split('x');
    if (params.size() < 2 || params[0].isEmpty() || params[1].isEmpty())
        return QSize();
    bool isDigit = params[0].at(0) >= '0' && params[0].at(0) <= '9';
    if (!isDigit)
        params[0] = params[0].mid(1);
    
    return QSize(params[0].trimmed().toInt(), params[1].trimmed().toInt());
}

QByteArray QnActiResource::unquoteStr(const QByteArray& v)
{
    QByteArray value = v.trimmed();
    int pos1 = value.startsWith('\'') ? 1 : 0;
    int pos2 = value.endsWith('\'') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

QByteArray QnActiResource::makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status, bool keepAllData) const
{
    QByteArray result;

    QUrl url(getUrl());
    QAuthenticator auth = getAuth();
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());
    QString pattern(QLatin1String("cgi-bin/%1?USER=%2&PWD=%3&%4"));
    status = client.doGET(pattern.arg(group).arg(auth.user()).arg(auth.password()).arg(command));
    if (status == CL_HTTP_SUCCESS) {
        client.readAll(result);
        if (result.startsWith("ERROR: bad account")) {
            status = CL_HTTP_AUTH_REQUIRED; 
            return QByteArray();
        }
    }
    if (keepAllData)
        return result;
    else
        return unquoteStr(result.mid(result.indexOf('=')+1).trimmed());
}

static bool resolutionGreaterThan(const QSize &s1, const QSize &s2)
{
    long long res1 = s1.width() * s1.height();
    long long res2 = s2.width() * s2.height();
    return res1 > res2? true: (res1 == res2 && s1.width() > s2.width()? true: false);
}

QList<QSize> QnActiResource::parseResolutionStr(const QByteArray& resolutions)
{
    QList<QSize> result;
    QList<QSize> availResolutions;
    foreach(const QByteArray& r, resolutions.split(','))
        result << extractResolution(r);
    qSort(result.begin(), result.end(), resolutionGreaterThan);
    return result;
}

QMap<QByteArray, QByteArray> QnActiResource::parseReport(const QByteArray& report) const
{
    QMap<QByteArray, QByteArray> result;
    QList<QByteArray> lines = report.split('\n');
    foreach(const QByteArray& line, lines) {
        QList<QByteArray> tmp = line.split('=');
        result.insert(tmp[0].trimmed().toLower(), tmp.size() >= 2 ? tmp[1].trimmed() : "");
    }

    return result;
}

QList<int> QnActiResource::parseVideoBitrateCap(const QByteArray& bitrateCap) const
{
    QList<int> result;
    foreach(QByteArray bitrate, bitrateCap.split(','))
    {
        bitrate = bitrate.trimmed().toUpper();
        int coeff = 1;
        if (bitrate.endsWith("M"))
            coeff = 1000;
        bitrate.chop(1);
        result << bitrate.toFloat()*coeff;
    }
    return result;
}

bool QnActiResource::initInternal()
{
    CLHttpStatus status;
        
    QByteArray resolutions= makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_RESOLUTION_CAP"), status);

    if (status == CL_HTTP_AUTH_REQUIRED) 
        setStatus(QnResource::Unauthorized);
    if (status != CL_HTTP_SUCCESS)
        return false;

    QByteArray serverReport = makeActiRequest(QLatin1String("system"), QLatin1String("SERVER_REPORT"), status, true);
    if (status != CL_HTTP_SUCCESS)
        return false;
    QMap<QByteArray, QByteArray> report = parseReport(serverReport);
    setFirmware(QString::fromUtf8(report.value("firmware version")));
    setMAC(QString::fromUtf8(report.value("mac address")));

    bool dualStreaming = report.value("channels").toInt() > 1;

    QList<QSize> availResolutions = parseResolutionStr(resolutions);
    if (availResolutions.isEmpty() || availResolutions[0].isEmpty())
        return false;

    m_resolution[0] = availResolutions.first();

    if (dualStreaming) {
        resolutions = makeActiRequest(QLatin1String("system"), QLatin1String("CHANNEL=2&VIDEO_RESOLUTION_CAP"), status);
        if (status == CL_HTTP_SUCCESS)
        {
            availResolutions = parseResolutionStr(resolutions);
            int maxSecondaryRes = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();

            float currentAspect = getResolutionAspectRatio(m_resolution[0]);
            m_resolution[1] = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect, maxSecondaryRes, availResolutions);
            if (m_resolution[1] == EMPTY_RESOLUTION_PAIR)
                m_resolution[1] = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSecondaryRes, availResolutions); // try to get resolution ignoring aspect ration
        }
    }

    QByteArray fpsString = makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_FPS_CAP"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;

    QList<QByteArray> fpsList = fpsString.split(';');
    
    for (int i = 0; i < MAX_STREAMS && i < fpsList.size(); ++i) {
        QList<QByteArray> fps = fpsList[i].split(',');
        foreach(const QByteArray& data, fps)
            m_availFps[i] << data.toInt();
        qSort(m_availFps[i]);
    }

    QByteArray rtspPortString = makeActiRequest(QLatin1String("system"), QLatin1String("V2_PORT_RTSP"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;
    m_rtspPort = rtspPortString.trimmed().toInt();
    if (m_rtspPort == 0)
        m_rtspPort = DEFAULT_RTSP_PORT;

    m_hasAudio = report.value("audio").toInt() > 0;

    QByteArray bitrateCap = report.value("video_bitrate_cap");
    if (!bitrateCap.isEmpty())
        m_availBitrate = parseVideoBitrateCap(bitrateCap);


    initializePtz();

    setParam(AUDIO_SUPPORTED_PARAM_NAME, m_hasAudio ? 1 : 0, QnDomainDatabase);
    setParam(MAX_FPS_PARAM_NAME, getMaxFps(), QnDomainDatabase);
    setParam(DUAL_STREAMING_PARAM_NAME, !m_resolution[1].isEmpty() ? 1 : 0, QnDomainDatabase);
    save();

    return true;
}

bool QnActiResource::isResourceAccessible()
{
    return updateMACAddress();
}

const QnResourceAudioLayout* QnActiResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnActiStreamReader* actiReader = dynamic_cast<const QnActiStreamReader*>(dataProvider);
        if (actiReader && actiReader->getDPAudioLayout())
            return actiReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

QString QnActiResource::getRtspUrl(int actiChannelNum) const
{
    QUrl url(getUrl());
    url.setScheme(QLatin1String("rtsp"));
    url.setPort(m_rtspPort);
    if (isAudioSupported())
        url.setPath(QString(QLatin1String("stream%1")).arg(actiChannelNum));
    else
        url.setPath(QString(QLatin1String("track%1")).arg(actiChannelNum));
    return url.toString();
}

int QnActiResource::getMaxFps()
{
    return m_availFps[0].last();
}

QSize QnActiResource::getResolution(QnResource::ConnectionRole role) const
{
    return (role == QnResource::Role_LiveVideo ? m_resolution[0] : m_resolution[1]);
}

int QnActiResource::roundFps(int srcFps, QnResource::ConnectionRole role) const
{
    QList<int> availFps = (role == QnResource::Role_LiveVideo ? m_availFps[0] : m_availFps[1]);
    int minDistance = INT_MAX;
    int result = srcFps;
    for (int i = 0; i < availFps.size(); ++i)
    {
        int distance = qAbs(availFps[i] - srcFps);
        if (distance <= minDistance) { // preffer higher fps if same distance
            minDistance = distance;
            result = availFps[i];
        }
    }

    return result;
}

int QnActiResource::roundBitrate(int srcBitrateKbps) const
{
    int minDistance = INT_MAX;
    int result = srcBitrateKbps;
    for (int i = 0; i < m_availBitrate.size(); ++i)
    {
        int distance = qAbs(m_availBitrate[i] - srcBitrateKbps);
        if (distance <= minDistance) { // preffer higher bitrate if same distance
            minDistance = distance;
            result = m_availBitrate[i];
        }
    }

    return result;
}

bool QnActiResource::isAudioSupported() const
{
    return m_hasAudio;
}

bool QnActiResource::hasDualStreaming() const
{
    QVariant mediaVariant;
    QnActiResource* this_casted = const_cast<QnActiResource*>(this);
    this_casted->getParam(DUAL_STREAMING_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toInt();
}

QnAbstractPtzController* QnActiResource::getPtzController()
{
    return m_ptzController.data();
}

void QnActiResource::initializePtz()
{
    m_ptzController.reset(new QnActiPtzController(this));
    Qn::CameraCapabilities capabilities = m_ptzController->getCapabilities();
    if(capabilities == Qn::NoCapabilities)
        m_ptzController.reset();

    setCameraCapabilities((getCameraCapabilities() & ~Qn::AllPtzCapabilities) | capabilities);
}
