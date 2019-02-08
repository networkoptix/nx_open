#include "transcode_media_encoder.h"

#include <nx/utils/log/log.h>

#include "stream_reader.h"
#include "transcode_stream_reader.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kDefaultSecondaryFps = 5;
static constexpr int kDefaultSecondaryWidth = 640;
static constexpr int kDefaultBitrate = 5000000;

}

TranscodeMediaEncoder::TranscodeMediaEncoder(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const std::shared_ptr<Camera>& camera)
    :
    MediaEncoder(
        parentRefManager,
        encoderIndex,
        camera)
{
}

int TranscodeMediaEncoder::getResolutionList(
    nxcip::ResolutionInfo* infoList,
    int* infoListCount) const
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    auto list = m_camera->resolutionList();
    if (list.empty())
    {
        *infoListCount = 0;
        return nxcip::NX_IO_ERROR;
    }

    fillResolutionList(list, infoList, infoListCount);

    CodecParameters secondary = calculateSecondaryCodecParams(list);

    int index = list.size() < nxcip::MAX_RESOLUTION_LIST_SIZE
        ? (int)list.size()
        : nxcip::MAX_RESOLUTION_LIST_SIZE - 1;

    infoList[index].resolution = secondary.resolution;
    infoList[index].maxFps = secondary.fps;

    *infoListCount = index + 1;
    return nxcip::NX_NO_ERROR;
}

int TranscodeMediaEncoder::setFps(const float& fps, float* selectedFps)
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    m_codecParams.fps = fps;
    if (m_streamReader)
        m_streamReader->setFps(fps);
    *selectedFps = fps;

    return nxcip::NX_NO_ERROR;
}

nxcip::StreamReader* TranscodeMediaEncoder::getLiveStreamReader()
{
    NX_DEBUG(
        this,
        "%1 : Secondary stream requested with codec params: %2",
        m_camera->info().modelName,
        m_codecParams.toString());

    if (!m_streamReader)
    {
        static constexpr bool kForceTranscoding = true;
        m_streamReader.reset(new StreamReader(
            &m_refManager,
            m_encoderIndex,
            m_codecParams,
            m_camera,
            kForceTranscoding));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

CodecParameters TranscodeMediaEncoder::calculateSecondaryCodecParams(
    const std::vector<device::video::ResolutionData>& resolutionList) const
{
    NX_ASSERT(resolutionList.size() > 0);
    if (resolutionList.empty())
        return {};

    const auto& resolution = resolutionList[resolutionList.size() - 1];
    float aspectRatio = (float) resolution.width / resolution.height;

    AVCodecID codecId = ffmpeg::utils::toAVCodecId(
        m_camera->compressionTypeDescriptor()->toNxCompressionType());

    return CodecParameters(
        codecId,
        kDefaultSecondaryFps,
        kDefaultBitrate,
        {kDefaultSecondaryWidth, (int)(kDefaultSecondaryWidth / aspectRatio)});
}

} // namespace nx
} // namespace usb_cam
