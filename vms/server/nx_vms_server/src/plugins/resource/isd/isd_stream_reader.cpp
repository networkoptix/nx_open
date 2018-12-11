#ifdef ENABLE_ISD

#include "isd_stream_reader.h"

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/deprecated/simple_http_client.h>

#include "isd_resource.h"


const int QnISDStreamReader::ISD_HTTP_REQUEST_TIMEOUT_MS = 5000;

extern QString getValueFromString(const QString& line);

QnISDStreamReader::QnISDStreamReader(
    const QnPlIsdResourcePtr& res)
    :
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res, res->getTimeOffset()),
    m_isdCam(res)
{
}

QnISDStreamReader::~QnISDStreamReader()
{
    stop();
}

QString QnISDStreamReader::serializeStreamParams(
    const QnLiveStreamParams& params,
    int profileIndex) const
{
    QString result;
    QTextStream t(&result);

    static const int kMinBitrate = 256; //< kbps
    const int desiredBitrateKbps = std::max(kMinBitrate, m_isdCam->suggestBitrateKbps(params, getRole()));

    t << "VideoInput.1.h264." << profileIndex << ".Resolution=" << params.resolution.width()
      << "x" << params.resolution.height() << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".FrameRate=" << params.fps << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateControl=vbr\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateVariableMin=" << desiredBitrateKbps / 5 << "\r\n";
    t << "VideoInput.1.h264." << profileIndex << ".BitrateVariableMax=" << desiredBitrateKbps / 5 * 6 << "\r\n";    //*1.2
    t.flush();
    return result;
}

CameraDiagnostics::Result QnISDStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& liveStreamParams)
{
    namespace StatusCode = nx::network::http::StatusCode;

    QnLiveStreamParams params = liveStreamParams;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);

    int port = QUrl(m_isdCam->getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT);
    const auto baseRequestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(m_isdCam->getHostAddress())
        .setPort(port)
        .toUrl();

    nx::network::http::HttpClient httpClient;
    httpClient.setUserName(m_isdCam->getAuth().user());
    httpClient.setUserPassword(m_isdCam->getAuth().password());

    QSize resolution;
    int profileIndex;
    if (role == Qn::CR_SecondaryLiveVideo)
    {
        resolution = m_isdCam->getSecondaryResolution();
        profileIndex = 2;
    }
    else
    {
        resolution = m_isdCam->getPrimaryResolution();
        profileIndex = 1;
    }

    params.resolution = resolution;

    if (isCameraControlRequired)
    {
        QString streamProfileStr = serializeStreamParams(params, profileIndex);

        const auto url = nx::network::url::Builder(baseRequestUrl).setPath("/api/param.cgi").toUrl();
        bool ok = httpClient.doPost(
            url, "Content-Type: application/x-www-form-urlencoded", streamProfileStr.toUtf8());
        if (!ok)
        {
            NX_DEBUG(this, "Request %1 system error: %2", url, httpClient.lastSysErrorCode());
        }
        else if (!StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
        {
            NX_DEBUG(this, "Request %1 error: %2", url,
                StatusCode::toString(httpClient.response()->statusLine.statusCode));
        }
        QnSleep::msleep(100);
    }

    const auto requestUrl = nx::network::url::Builder(baseRequestUrl)
            .setPath("/api/param.cgi")
            .setQuery(lm("req=VideoInput.1.h264.%1.Rtsp.AbsolutePath").arg(profileIndex))
            .toUrl();

    if (!httpClient.doGet(requestUrl))
        return CameraDiagnostics::IOErrorResult(requestUrl.toString());

    const auto statusCode = StatusCode::Value(httpClient.response()->statusLine.statusCode);
    if (!StatusCode::isSuccessCode(statusCode))
        NX_DEBUG(this, "Request %1 failed with %2", requestUrl, StatusCode::toString(statusCode));

    if (statusCode == nx::network::http::StatusCode::unauthorized)
        return CameraDiagnostics::NotAuthorisedResult(requestUrl.toString());

    auto messageBody = httpClient.fetchEntireMessageBody();
    QString rtspUrl;
    if (messageBody)
        rtspUrl = getValueFromString(*messageBody);

    QStringList urlLst = rtspUrl.split(QLatin1Char('\r'), QString::SkipEmptyParts);
    if(urlLst.size() < 1)
        return CameraDiagnostics::NoMediaTrackResult(requestUrl.toString());

    rtspUrl = urlLst.at(0);

    if (rtspUrl.isEmpty())
        return CameraDiagnostics::NoMediaTrackResult(requestUrl.toString());

    m_isdCam->updateSourceUrl(rtspUrl, getRole());
    NX_INFO(this, "Got stream URL %1 for camera %2 for role %3", rtspUrl, m_resource->getUrl(), getRole());

    m_rtpStreamParser.setRequest(rtspUrl);
    m_isdCam->updateSourceUrl(m_rtpStreamParser.getCurrentStreamUrl(), getRole());
    return m_rtpStreamParser.openStream();
}

void QnISDStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool QnISDStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr QnISDStreamReader::getCameraMetadata()
{
    return QnMetaDataV1Ptr(0);
}



void QnISDStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_rtpStreamParser.pleaseStop();
}

QnAbstractMediaDataPtr QnISDStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez)
        {
            //QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(rez);
            //ToDo: if (videoData)
            //    parseMotionInfo(videoData);

            //if (!videoData || isGotFrame(videoData))
            break;
        }
        else {
            errorCount++;
            if (errorCount > 1) {
                closeStream();
                break;
            }
        }
    }

    return rez;
}

QnConstResourceAudioLayoutPtr QnISDStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

#endif // #ifdef ENABLE_ISD
