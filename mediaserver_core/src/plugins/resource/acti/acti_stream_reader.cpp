#ifdef ENABLE_ACTI

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>
#include <network/tcp_connection_priv.h>

#include "acti_resource.h"
#include "acti_stream_reader.h"

QnActiStreamReader::QnActiStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_multiCodec(res)
{
    m_actiRes = res.dynamicCast<QnActiResource>();
}

QnActiStreamReader::~QnActiStreamReader()
{
    stop();
}

int QnActiStreamReader::getActiChannelNum() const
{
    return m_role == Qn::CR_LiveVideo ? 1 : 2;
}

int QnActiStreamReader::toJpegQuality(const QnLiveStreamParams& params)
{
    auto quality = params.quality;
    if (quality == Qn::StreamQuality::QualityNotDefined)
    {
        int srcBitrate = params.bitrateKbps;
        int bitrateDelta = std::numeric_limits<int>::max();
        for (int i = Qn::StreamQuality::QualityLowest; i <= Qn::StreamQuality::QualityHighest; ++i)
        {
            QnLiveStreamParams p(params);
            p.quality = (Qn::StreamQuality) i;
            int bitrate = m_actiRes->suggestBitrateForQualityKbps((Qn::StreamQuality) i, params.resolution, params.fps, getRole());
            if (abs(bitrate - srcBitrate) < bitrateDelta)
            {
                quality = p.quality;
                bitrateDelta = abs(bitrate - srcBitrate);
            }
        }
    }
    switch (quality)
    {
        case Qn::QualityLowest:
            return 15;
        case Qn::QualityLow:
            return 30;
        case Qn::QualityNormal:
            return 50;
        case Qn::QualityHigh:
            return 70;
        case Qn::QualityHighest:
            return 80;
        default:
            return 50;
    }
}

CameraDiagnostics::Result QnActiStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& streamParams)
{
    // configure stream params
    QnLiveStreamParams params = streamParams;
    QString SET_RESOLUTION(QLatin1String("CHANNEL=%1&VIDEO_RESOLUTION=%2"));
    QString SET_FPS(QLatin1String("CHANNEL=%1&VIDEO_FPS_NUM=%2"));
    QString SET_BITRATE(QLatin1String("CHANNEL=%1&VIDEO_BITRATE=%2&VIDEO_MAX_BITRATE=%2"));
    QString SET_QUALITY(QLatin1String("CHANNEL=%1&VIDEO_MJPEG_QUALITY=%2"));
    QString SET_ENCODER(QLatin1String("CHANNEL=%1&VIDEO_ENCODER=%2"));
    QString SET_STREAMING_METHOD(QLatin1String("CHANNEL=%1&STREAMING_METHOD_CURRENT=%2"));

    const int kStreamingMethodTcpOnly = 0;
    const int kStreamingMethodMulticast = 1;
    const int kStreamingMethodRtpUdp = 3;
    const int kStreamingMethodRtpMulticast = 4;
    const int kStreamingMethodRtpUdpMulticast = 5;

    m_multiCodec.setRole(m_role);
    params.fps = m_actiRes->roundFps(params.fps, m_role);
    int ch = getActiChannelNum();

    QString resolutionStr = QnActiResource::formatResolutionStr(params.resolution);

    int bitrate = m_actiRes->suggestBitrateKbps(params, getRole());
    bitrate = m_actiRes->roundBitrate(bitrate);
    QString bitrateStr = m_actiRes->formatBitrateString(bitrate);

    QString encoderStr = QnActiResource::toActiEncoderString(params.codec);
    if (encoderStr.isEmpty())
        return CameraDiagnostics::CannotConfigureMediaStreamResult(lit("encoder"));

    auto desiredTransport = m_actiRes->getDesiredTransport();

    if (isCameraControlRequired)
    {
        CLHttpStatus status;
        if (desiredTransport == nx::vms::api::RtpTransportType::udp)
        {
            QByteArray result = m_actiRes->makeActiRequest(
                QLatin1String("encoder"),
                SET_STREAMING_METHOD
                .arg(ch)
                .arg(kStreamingMethodRtpUdp),
                status);

            if (status != CL_HTTP_SUCCESS)
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("streaming method"));
        }

        QByteArray result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_FPS.arg(ch).arg(params.fps), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_RESOLUTION.arg(ch).arg(resolutionStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));

        if (params.codec == "MJPEG")
            result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_QUALITY.arg(ch).arg(toJpegQuality(params)), status);
        else
            result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_BITRATE.arg(ch).arg(bitrateStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("bitrate"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_ENCODER.arg(ch).arg(encoderStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("encoder"));

        if (!m_actiRes->SetupAudioInput())
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));
    }

    // get URL

    QString streamUrl = m_actiRes->getRtspUrl(ch);

    NX_LOG(lit("got stream URL %1 for camera %2 for role %3")
        .arg(streamUrl)
        .arg(m_resource->getUrl())
        .arg(getRole()),
        cl_logINFO);

    m_multiCodec.setRequest(streamUrl);
	m_actiRes->updateSourceUrl(m_multiCodec.getCurrentStreamUrl(), getRole());
    m_multiCodec.setRtpTransport(desiredTransport);
    const CameraDiagnostics::Result result = m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(Qn::Unauthorized);

    return result;
}

void QnActiStreamReader::closeStream()
{
    m_multiCodec.closeStream();
}

bool QnActiStreamReader::isStreamOpened() const
{
    return m_multiCodec.isStreamOpened();
}


void QnActiStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_multiCodec.pleaseStop();
}

QnAbstractMediaDataPtr QnActiStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (!rez)
        closeStream();

    return rez;
}

QnConstResourceAudioLayoutPtr QnActiStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}

#endif // #ifdef ENABLE_ACTI
