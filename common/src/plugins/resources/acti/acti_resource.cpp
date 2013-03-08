
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


using namespace std;

const char* QnActiResource::MANUFACTURE = "ACTI";
static const int TCP_TIMEOUT = 3000;
static const int DEFAULT_RTSP_PORT = 7070;

QString AUDIO_SUPPORTED_PARAM_NAME = QLatin1String("isAudioSupported");
QString DUAL_STREAMING_PARAM_NAME = QLatin1String("hasDualStreaming");

QnActiResource::QnActiResource():
    m_hasDualStreaming(false),
    m_rtspPort(DEFAULT_RTSP_PORT)
{
    setAuth(QLatin1String("admin"), QLatin1String("123456"));
}

QnActiResource::~QnActiResource()
{
}

QString QnActiResource::manufacture() const
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

QByteArray unquoteStr(const QByteArray& value)
{
    int pos1 = value.startsWith('\'') ? 1 : 0;
    int pos2 = value.endsWith('\'') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

QByteArray QnActiResource::makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status) const
{
    QByteArray result;

    QUrl url(getUrl());
    QAuthenticator auth = getAuth();
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());
    QString pattern(QLatin1String("cgi-bin/%1?USER=%2&PWD=%3&%4"));
    status = client.doGET(pattern.arg(group).arg(auth.user()).arg(auth.password()).arg(command));
    if (status == CL_HTTP_SUCCESS)
        client.readAll(result);
    return unquoteStr(result.mid(command.size()+1).trimmed());
}

bool QnActiResource::initInternal()
{
    CLHttpStatus status;
        
    QByteArray resolutions= makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_RESOLUTION_CAP"), status);

    if (status == CL_HTTP_AUTH_REQUIRED) 
        setStatus(QnResource::Unauthorized);
    if (status != CL_HTTP_SUCCESS)
        return false;

    QList<QByteArray> resList = resolutions.split(',');
    m_hasDualStreaming = resList.size() > 1;
    m_maxResolution[0] = extractResolution(resList[0]);
    if (m_hasDualStreaming)
        m_maxResolution[1] = extractResolution(resList[1]);
    if (m_maxResolution[0].isEmpty())
        return false;

    QByteArray fpsString = makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_FPS_CAP"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;

    QList<QByteArray> fpsList = fpsString.split(';');
    
    for (int i = 0; i < MAX_STREAMS && i < fpsList.size(); ++i) {
        QList<QByteArray> fps = fpsList[i].split(',');
        foreach(const QByteArray& data, fps)
            m_availFps[i] << data.toInt();
    }

    QByteArray rtspPortString = makeActiRequest(QLatin1String("system"), QLatin1String("V2_PORT_RTSP"), status);
    if (status != CL_HTTP_SUCCESS)
        return false;
    m_rtspPort = rtspPortString.trimmed().toInt();
    if (m_rtspPort == 0)
        m_rtspPort = DEFAULT_RTSP_PORT;

    setParam(DUAL_STREAMING_PARAM_NAME, m_hasDualStreaming ? 1 : 0, QnDomainDatabase);
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
    url.setPath(QString(QLatin1String("track%1")).arg(actiChannelNum));
    return url.toString();
}
