#include "transcode_media_encoder.h"

#include "stream_reader.h"
#include "transcode_stream_reader.h"

namespace nx {
namespace usb_cam {

namespace {

int constexpr kTargetFps = 5;
int constexpr kTargetWidth = 640;
int constexpr kDefaultBitrate = 5000000;

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
    std::cout << "TranscodeMediaEncoder" << std::endl;
}

TranscodeMediaEncoder::~TranscodeMediaEncoder()
{
    std::cout << "~TranscodeMediaEncoder" << std::endl;
}

int TranscodeMediaEncoder::getResolutionList(nxcip::ResolutionInfo* infoList, int* infoListCount) const
{
    auto list = m_camera->getResolutionList();
    if (list.empty())
    {
        *infoListCount = 0;
        return nxcip::NX_OTHER_ERROR;
    }

    fillResolutionList(list, infoList, infoListCount);

    CodecParameters secondary = calculateSecondaryCodecParams(list);

    int index = list.size() < nxcip::MAX_RESOLUTION_LIST_SIZE
        ? list.size()
        : nxcip::MAX_RESOLUTION_LIST_SIZE - 1;

    infoList[index].resolution.width = secondary.width;
    infoList[index].resolution.height = secondary.height;
    infoList[index].maxFps = secondary.fps;

    *infoListCount = index + 1;
    return nxcip::NX_NO_ERROR;
}

int TranscodeMediaEncoder::setFps(const float& fps, float* selectedFps)
{
    m_codecParams.fps = fps;
    if (m_streamReader)
        m_streamReader->setFps(fps);
    *selectedFps = fps;
    return nxcip::NX_NO_ERROR;
}

nxcip::StreamReader* TranscodeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        std::unique_ptr<StreamReaderPrivate> transcoder = std::make_unique<TranscodeStreamReader>(
            m_encoderIndex,
            m_codecParams,
            m_camera);
        
        m_streamReader.reset(new StreamReader(
            &m_refManager,
            std::move(transcoder)));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

CodecParameters TranscodeMediaEncoder::calculateSecondaryCodecParams(
    const std::vector<device::ResolutionData>& resolutionList) const
{
    NX_ASSERT(resolutionList.size() > 0);
    if (resolutionList.empty())
        return CodecParameters(AV_CODEC_ID_NONE, 30, kDefaultBitrate, kTargetWidth, kTargetWidth*9/16);

    const auto& resolutionData = resolutionList[0];
    float aspectRatio = (float) resolutionData.width / resolutionData.height;

    AVCodecID codecID = ffmpeg::utils::toAVCodecID(
        m_camera->compressionTypeDescriptor()->toNxCompressionType());

    return CodecParameters(
        codecID,
        kTargetFps,
        kDefaultBitrate,
        kTargetWidth,
        kTargetWidth / aspectRatio);
}

} // namespace nx
} // namespace usb_cam