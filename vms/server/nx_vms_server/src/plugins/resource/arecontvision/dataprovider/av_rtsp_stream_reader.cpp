/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_ARECONT

#include "av_rtsp_stream_reader.h"

#include <nx/network/rtsp/rtsp_types.h>
#include <nx/utils/log/log.h>
#include <plugins/resource/arecontvision/resource/av_panoramic.h>

QnArecontRtspStreamReader::QnArecontRtspStreamReader(
    const QnPlAreconVisionResourcePtr& res)
    :
    parent_type(res),
    m_rtpStreamParser(res, res->getTimeOffset())
{
    m_metaReader = std::make_unique<ArecontMetaReader>(
        res->getVideoLayout(0)->channelCount(),
        std::chrono::milliseconds(META_DATA_DURATION_MS),
        META_FRAME_INTERVAL);
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

    // Override ssn param since some cameras does not support ssn > 8
    m_streamParam.insert("streamID", getRole() == Qn::CR_LiveVideo ? 1 : 2);

    QString requestStr;
    {
        QnMutexLocker lk(&m_mutex);
        requestStr = m_camera->generateRequestString(
            m_streamParam,
            m_camera->isH264(),
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

    int channels = 1;
    const auto resource = getResource().dynamicCast<QnPlAreconVisionResource>();
    NX_CRITICAL(resource);
    if (auto layout = resource->getVideoLayout())
        channels = layout->channelCount();

    const auto maxResolution = getMaxSensorSize();
    if (getRole() == Qn::CR_SecondaryLiveVideo)
    {
        params.resolution = QSize(maxResolution.width() / 2, maxResolution.height() / 2);
        requestStr += lit("&FPS=%1").arg((int)params.fps);
        const int desiredBitrateKbps = m_camera->suggestBitrateKbps(
            params,
            getRole()) * channels;
        requestStr += lit("&Ratelimit=%1").arg(desiredBitrateKbps);
    }
    else
    {
        requestStr += lit("&FPS=%1").arg((int)params.fps);
        params.resolution = QSize(maxResolution.width(), maxResolution.height());
        const int desiredBitrateKbps = m_camera->suggestBitrateKbps(
            params,
            getRole()) * channels;
        requestStr += lit("&Ratelimit=%1").arg(desiredBitrateKbps);
    }
    if (m_camera->isAudioEnabled())
        requestStr += lit("&MIC=on");

    const QString url = lit("rtsp://%1:%2/%3").arg(m_camera->getHostAddress()).arg(
        nx::network::rtsp::DEFAULT_RTSP_PORT).arg(requestStr);

    m_rtpStreamParser.setRequest(url);
    m_camera->updateSourceUrl(m_rtpStreamParser.getCurrentStreamUrl(), getRole());
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
    auto motion = m_metaReader->getData();
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

// Override needMetaData due to we have async method of meta data obtaining in hardware case
bool QnArecontRtspStreamReader::needMetadata()
{
    if (!parent_type::needHardwareMotion())
        return parent_type::needMetadata();

    if (m_metaReader->hasData())
        return true;

    auto resource = getResource().dynamicCast<QnPlAreconVisionResource>();
    NX_ASSERT(resource);
    m_metaReader->requestIfReady(resource.data());
    return false;
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
    m_metaReader->onNewFrame();
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
