#ifdef ENABLE_ACTI

#include <QtCore/QTextStream>

#include <nx/utils/log/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>
#include <network/tcp_connection_priv.h>

#include "acti_resource.h"
#include "acti_stream_reader.h"

QnActiStreamReader::QnActiStreamReader(
    const QnActiResourcePtr& res)
    :
    CLServerPushStreamReader(res),
    m_multiCodec(res, res->getTimeOffset()),
    m_actiRes(res)
{
}

QnActiStreamReader::~QnActiStreamReader()
{
    stop();
}

int QnActiStreamReader::getActiChannelNum() const
{
    return getRole() == Qn::CR_LiveVideo ? 1 : 2;
}

int QnActiStreamReader::toJpegQuality(const QnLiveStreamParams& params)
{
    auto quality = params.quality;
    if (quality == Qn::StreamQuality::undefined)
    {
        int srcBitrate = params.bitrateKbps;
        int bitrateDelta = std::numeric_limits<int>::max();
        for (int i = (int)Qn::StreamQuality::lowest; i <= (int)Qn::StreamQuality::highest; ++i)
        {
            QnLiveStreamParams p(params);
            p.quality = (Qn::StreamQuality) i;
            int bitrate = m_actiRes->suggestBitrateForQualityKbps((Qn::StreamQuality) i,
                params.resolution, params.fps, params.codec, getRole());
            if (abs(bitrate - srcBitrate) < bitrateDelta)
            {
                quality = p.quality;
                bitrateDelta = abs(bitrate - srcBitrate);
            }
        }
    }
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
            return 15;
        case Qn::StreamQuality::low:
            return 30;
        case Qn::StreamQuality::normal:
            return 50;
        case Qn::StreamQuality::high:
            return 70;
        case Qn::StreamQuality::highest:
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

    m_multiCodec.setRole(getRole());
    params.fps = m_actiRes->roundFps(params.fps, getRole());
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
        nx::network::http::StatusCode::Value status;
        if (desiredTransport == nx::vms::api::RtpTransportType::udp)
        {
            QByteArray result = m_actiRes->makeActiRequest(
                QLatin1String("encoder"),
                SET_STREAMING_METHOD
                .arg(ch)
                .arg(kStreamingMethodRtpUdp),
                status);

            if (!nx::network::http::StatusCode::isSuccessCode(status))
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("streaming method"));
        }

        QByteArray result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_FPS.arg(ch).arg(params.fps), status);
        if (!nx::network::http::StatusCode::isSuccessCode(status))
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_RESOLUTION.arg(ch).arg(resolutionStr), status);
        if (!nx::network::http::StatusCode::isSuccessCode(status))
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));

        if (params.codec == "MJPEG")
            result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_QUALITY.arg(ch).arg(toJpegQuality(params)), status);
        else
            result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_BITRATE.arg(ch).arg(bitrateStr), status);
        if (!nx::network::http::StatusCode::isSuccessCode(status))
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("bitrate"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_ENCODER.arg(ch).arg(encoderStr), status);
        if (!nx::network::http::StatusCode::isSuccessCode(status))
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("encoder"));

        if (!m_actiRes->SetupAudioInput())
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));
    }

    // get URL

    QString streamUrl = m_actiRes->getRtspUrl(ch);

    NX_INFO(this, lit("got stream URL %1 for camera %2 for role %3")
        .arg(streamUrl)
        .arg(m_resource->getUrl())
        .arg(getRole()));

    m_multiCodec.setRequest(streamUrl);
    m_actiRes->updateSourceUrl(m_multiCodec.getCurrentStreamUrl(), getRole());
    m_multiCodec.setRtpTransport(desiredTransport);
    const CameraDiagnostics::Result result = m_multiCodec.openStream();
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

    if (needMetadata())
        return getMetadata();

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
