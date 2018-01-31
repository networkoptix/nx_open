/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ARECONT

#include "av_rtsp_stream_reader.h"

#include <nx/network/rtsp/rtsp_types.h>
#include <nx/utils/log/log.h>
#include <plugins/resource/arecontvision/resource/av_panoramic.h>


QnArecontRtspStreamReader::QnArecontRtspStreamReader(const QnResourcePtr& res)
:
    parent_type(res),
    m_rtpStreamParser(res)
{
}

QnArecontRtspStreamReader::~QnArecontRtspStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnArecontRtspStreamReader::openStreamInternal(
    bool /*isCameraControlRequired*/,
    const QnLiveStreamParams& liveStreamParams)
{
    QnLiveStreamParams params = liveStreamParams;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    const Qn::ConnectionRole role = getRole();
    m_rtpStreamParser.setRole(role);

    auto res = getResource().dynamicCast<QnPlAreconVisionResource>();

    QString requestStr;
    {
        QnMutexLocker lk(&m_mutex);
        requestStr = res->generateRequestString(
            m_streamParam,
            res->isH264(),
            role != Qn::CR_SecondaryLiveVideo,
            false,  //blackWhite
            NULL,   //outQuality,
            NULL);  //outResolution
        requestStr.replace(lit("h264"), lit("h264.sdp"));
        requestStr.replace(lit(";"), lit("&"));

        //requestStr = generateRequestString(
        //    params,
        //    res);
    }

    // TODO: advanced params control is not implemented for this driver yet
    const auto maxResolution = getMaxSensorSize();
    if (getRole() == Qn::CR_SecondaryLiveVideo)
    {
        params.resolution = QSize(maxResolution.width() / 2, maxResolution.height() / 2);
        requestStr += lit("&FPS=%1").arg((int)params.fps);
        const int desiredBitrateKbps = res->suggestBitrateKbps(
            params,
            getRole());
        requestStr += lit("&Ratelimit=%1").arg(desiredBitrateKbps);
    }
    else
    {
        requestStr += lit("&FPS=%1").arg((int)params.fps);
        params.resolution = QSize(maxResolution.width(), maxResolution.height());
        const int desiredBitrateKbps = res->suggestBitrateKbps(
            params,
            getRole());
        requestStr += lit("&Ratelimit=%1").arg(desiredBitrateKbps);
    }
    if (res->isAudioEnabled())
        requestStr += lit("&MIC=on");

    const QString url = lit("rtsp://%1:%2/%3").arg(res->getHostAddress()).arg(nx_rtsp::DEFAULT_RTSP_PORT).arg(requestStr);

    m_rtpStreamParser.setRequest(url);
	res->updateSourceUrl(m_rtpStreamParser.getCurrentStreamUrl(), getRole());
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

void QnArecontRtspStreamReader::pleaseReopenStream()
{
    parent_type::pleaseReopenStream();
    CLServerPushStreamReader::pleaseReopenStream();
}

QnAbstractMediaDataPtr QnArecontRtspStreamReader::getNextData()
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

void QnArecontRtspStreamReader::beforeRun()
{
    parent_type::beforeRun();
    QnArecontPanoramicResourcePtr res = getResource().dynamicCast<QnArecontPanoramicResource>();
    if (res)
        res->updateFlipState();
}

#endif // ENABLE_ARECONT
