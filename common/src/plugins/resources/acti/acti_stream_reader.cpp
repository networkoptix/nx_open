#include <QTextStream>
#include "acti_resource.h"
#include "acti_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "utils/network/tcp_connection_priv.h"

QnActiStreamReader::QnActiStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
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
    return m_role == QnResource::Role_LiveVideo ? 1 : 2;
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

void QnActiStreamReader::openStream()
{
    // configure stream params

    QString SET_RESOLUTION(QLatin1String("CHANNEL=%1&VIDEO_RESOLUTION=%2"));
    QString SET_FPS(QLatin1String("CHANNEL=%1&VIDEO_FPS_NUM=%2"));
    QString SET_BITRATE(QLatin1String("CHANNEL=%1&VIDEO_BITRATE=%2"));
    QString SET_ENCODER(QLatin1String("CHANNEL=%1&VIDEO_ENCODER=%2"));
    QString SET_AUDIO(QLatin1String("CHANNEL=%1&V2_AUDIO_ENABLED=%2"));

    int fps = m_actiRes->roundFps(getFps(), m_role);
    int ch = getActiChannelNum();
    QSize resolution = m_actiRes->getResolution(m_role);
    QString resolutionStr = formatResolutionStr(resolution);
    int bitrate = m_actiRes->suggestBitrateKbps(getQuality(), resolution, fps);
    bitrate = m_actiRes->roundBitrate(bitrate);
    QString bitrateStr = formatBitrateStr(bitrate);
    QString encoderStr(QLatin1String("H264"));
    QString audioStr = m_actiRes->isAudioEnabled() ? QLatin1String("1") : QLatin1String("0");

    CLHttpStatus status;
    QByteArray result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_FPS.arg(ch).arg(fps), status);
    if (status != CL_HTTP_SUCCESS)
        return;

    result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_RESOLUTION.arg(ch).arg(resolutionStr), status);
    if (status != CL_HTTP_SUCCESS)
        return;

    result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_BITRATE.arg(ch).arg(bitrateStr), status);
    if (status != CL_HTTP_SUCCESS)
        return;

    result = m_actiRes->makeActiRequest(QLatin1String("encoder"), SET_ENCODER.arg(ch).arg(encoderStr), status);
    if (status != CL_HTTP_SUCCESS)
        return;

    if (m_actiRes->isAudioSupported())
    {
        result = m_actiRes->makeActiRequest(QLatin1String("system"), SET_AUDIO.arg(ch).arg(audioStr), status);
        if (status != CL_HTTP_SUCCESS)
            return;
    }


    // get URL

    QString streamUrl = m_actiRes->getRtspUrl(ch);

    m_multiCodec.setRequest(streamUrl);
    m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(QnResource::Unauthorized);

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
    QnLongRunnable::pleaseStop();
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

const QnResourceAudioLayout* QnActiStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}
