#ifdef ENABLE_ACTI

#include <QtCore/QTextStream>

#include <utils/common/log.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>
#include <utils/network/tcp_connection_priv.h>

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

QString QnActiStreamReader::formatBitrateStr(int bitrateKbps) const
{
    if (bitrateKbps < 1000)
        return QString(QLatin1String("%1K")).arg(bitrateKbps);
    else
        return QString(QLatin1String("%1.%2M")).arg(bitrateKbps/1000).arg((bitrateKbps%1000)/100);
}

QString QnActiStreamReader::formatResolutionStr(const QSize& resolution) const
{
    return QString(QLatin1String("N%1x%2")).arg(resolution.width()).arg(resolution.height());
}

CameraDiagnostics::Result QnActiStreamReader::openStream()
{
    // configure stream params

    QString SET_RESOLUTION(QLatin1String("CHANNEL=%1&VIDEO_RESOLUTION=%2"));
    QString SET_FPS(QLatin1String("CHANNEL=%1&VIDEO_FPS_NUM=%2"));
    QString SET_BITRATE(QLatin1String("CHANNEL=%1&VIDEO_BITRATE=%2"));
    QString SET_ENCODER(QLatin1String("CHANNEL=%1&VIDEO_ENCODER=%2"));
    QString SET_AUDIO(QLatin1String("CHANNEL=%1&V2_AUDIO_ENABLED=%2"));

    m_multiCodec.setRole(m_role);
    int fps = m_actiRes->roundFps(getFps(), m_role);
    int ch = getActiChannelNum();
    QSize resolution = m_actiRes->getResolution(m_role);
    QString resolutionStr = formatResolutionStr(resolution);
    int bitrate = m_actiRes->suggestBitrateKbps(getQuality(), resolution, fps);
    bitrate = m_actiRes->roundBitrate(bitrate);
    QString bitrateStr = formatBitrateStr(bitrate);
    QString encoderStr(QLatin1String("H264"));
    QString audioStr = m_actiRes->isAudioEnabled() ? QLatin1String("1") : QLatin1String("0");
    if (isCameraControlRequired())
    {
        CLHttpStatus status;
        QByteArray result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_FPS.arg(ch).arg(fps), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("fps"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_RESOLUTION.arg(ch).arg(resolutionStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("resolution"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_BITRATE.arg(ch).arg(bitrateStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("bitrate"));

        result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_ENCODER.arg(ch).arg(encoderStr), status);
        if (status != CL_HTTP_SUCCESS)
            return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("encoder"));

        if (m_actiRes->isAudioSupported())
        {
            result = m_actiRes->makeActiRequest(QLatin1String("system"), SET_AUDIO.arg(ch).arg(audioStr), status);
            if (status != CL_HTTP_SUCCESS)
                return CameraDiagnostics::CannotConfigureMediaStreamResult(QLatin1String("audio"));
        }
    }

    // get URL

    QString streamUrl = m_actiRes->getRtspUrl(ch);
    NX_LOG(lit("got stream URL %1 for camera %2 for role %3").arg(streamUrl).arg(m_resource->getUrl()).arg(getRole()), cl_logINFO);

    m_multiCodec.setRequest(streamUrl);
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
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (!rez)
        closeStream();

    return rez;
}

void QnActiStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnActiStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

QnConstResourceAudioLayoutPtr QnActiStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}

#endif // #ifdef ENABLE_ACTI
