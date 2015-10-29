/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ARECONT

#include "av_rtsp_stream_reader.h"

#include <utils/common/log.h>


QnArecontRtspStreamReader::QnArecontRtspStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res),
    m_rtpStreamParser(res)
{
}

QnArecontRtspStreamReader::~QnArecontRtspStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnArecontRtspStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    const Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);

    auto res = getResource().dynamicCast<QnPlAreconVisionResource>();

    const auto requestStr = generateRequestString(
        params,
        res);
    const QString url = lit("rtsp://%1:%2/%3").arg(res->getHostAddress()).arg(554).arg(requestStr);

    m_rtpStreamParser.setRequest(url);
    return m_rtpStreamParser.openStream();
}

void QnArecontRtspStreamReader::closeStream()
{
    m_rtpStreamParser.closeStream();
}

bool QnArecontRtspStreamReader::isStreamOpened() const
{
    return m_rtpStreamParser.isStreamOpened();
}

QnMetaDataV1Ptr QnArecontRtspStreamReader::getCameraMetadata()
{
    auto motion = static_cast<QnPlAreconVisionResource*>(getResource().data())->getCameraMetadata();
    if (!motion)
        return motion;
    filterMotionByMask(motion);
    return motion;
}

void QnArecontRtspStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    m_rtpStreamParser.pleaseStop();
}

QnAbstractMediaDataPtr QnArecontRtspStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    int errorCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        rez = m_rtpStreamParser.getNextData();
        if (rez) 
            break;

        errorCount++;
        if (errorCount > 1) {
            closeStream();
            break;
        }
    }

    return rez;
}

QnConstResourceAudioLayoutPtr QnArecontRtspStreamReader::getDPAudioLayout() const
{
    return m_rtpStreamParser.getAudioLayout();
}

QString QnArecontRtspStreamReader::generateRequestString(
    const QnLiveStreamParams& params,
    const QnPlAreconVisionResourcePtr& res)
{
    static const int SECOND_STREAM_FPS = 5;
    static const int SECOND_STREAM_BITRATE_KBPS = 512;

    const int maxWidth = res->getProperty(lit("MaxSensorWidth")).toInt();
    const int maxHeight = res->getProperty(lit("MaxSensorHeight")).toInt();

    QString str;

    str += lit("h264.sdp?");
    if (getRole() == Qn::CR_SecondaryLiveVideo)
    {
        str += lit("Res=half");
        str += lit("&FPS=%1").arg(SECOND_STREAM_FPS);
        str += lit("&QP=37");
        str += lit("&Ratelimit=%1").arg(SECOND_STREAM_BITRATE_KBPS);
    }
    else
    {
        str += lit("Res=full");
        str += lit("&FPS=%1").arg((int)params.fps);
        const int desiredBitrateKbps = res->suggestBitrateKbps(
            params.quality,
            QSize(maxWidth, maxHeight),
            params.fps);
        str += lit("&Ratelimit=%1").arg(desiredBitrateKbps);
    }
    str += lit("&x0=%1&y0=%2&x1=%3&y1=%4").arg(0).arg(0).arg(maxWidth).arg(maxHeight);
    str += lit("&ssn=%1").arg(rand());
    if (res->isAudioEnabled())
        str += lit("&MIC=on");

    return str;
}

#endif // ENABLE_ARECONT
