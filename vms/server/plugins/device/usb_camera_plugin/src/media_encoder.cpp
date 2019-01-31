#include "media_encoder.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

#include "camera/default_audio_encoder.h"
#include "camera_manager.h"
#include "device/video/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "stream_reader.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kBytesInOneKilobyte = 1000;

}

MediaEncoder::MediaEncoder(nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const std::shared_ptr<Camera>& camera)
    :
    m_refManager(parentRefManager),
    m_encoderIndex(encoderIndex),
    m_camera(camera),
    m_codecParams(camera->defaultVideoParameters())
{
}

void* MediaEncoder::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(
        &interfaceID,
        &nxcip::IID_CameraMediaEncoder2,
            sizeof(nxcip::IID_CameraMediaEncoder2)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if (memcmp(&interfaceID, &nxcip::IID_CameraMediaEncoder,
            sizeof(nxcip::IID_CameraMediaEncoder)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int MediaEncoder::addRef() const
{
    return m_refManager.addRef();
}

int MediaEncoder::releaseRef() const
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl(char* urlBuf) const
{
    urlBuf[0] = '\0'; //< the plugin does not broadcast it's own stream, the mediaserver does it.
    return nxcip::NX_NO_DATA;
}

int MediaEncoder::getMaxBitrate(int* maxBitrate) const
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    int bitrate =
        device::video::getMaxBitrate(m_camera->ffmpegUrl(), m_camera->compressionTypeDescriptor());

    *maxBitrate = bitrate / kBytesInOneKilobyte;

    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution(const nxcip::Resolution& resolution)
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    m_codecParams.resolution = resolution;
    if (m_streamReader)
        m_streamReader->setResolution(resolution);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps(const float& fps, float* selectedFps)
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    auto resolutionList = m_camera->resolutionList();
    if (resolutionList.empty())
        return nxcip::NX_OTHER_ERROR;

    std::sort(resolutionList.begin(), resolutionList.end(),
        [&](const device::video::ResolutionData& a, const device::video::ResolutionData& b) {
            return a.fps < b.fps;
        });

    float actualFps = 0;
    for (const auto& resolutionData : resolutionList)
    {
        if (resolutionData.fps >= fps)
        {
            actualFps = resolutionData.fps;
            break;
        }
    }

    NX_ASSERT(actualFps != 0);
    if (!actualFps)
        actualFps = 30;

    *selectedFps = actualFps;
    m_codecParams.fps = actualFps;
    if (m_streamReader)
        m_streamReader->setFps(actualFps);

    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate(int bitrateKbps, int* selectedBitrateKbps)
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    // the plugin uses bits per second internally, so convert to that first
    int bitrate = bitrateKbps * 1000;
    m_codecParams.bitrate = bitrate;
    if (m_streamReader)
        m_streamReader->setBitrate(bitrate);
    *selectedBitrateKbps = bitrateKbps;
    return nxcip::NX_NO_ERROR;
}
int MediaEncoder::getAudioFormat(nxcip::AudioFormat* audioFormat) const
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    int ffmpegError = 0;
    std::unique_ptr<ffmpeg::Codec> encoder = getDefaultAudioEncoder(&ffmpegError);
    if (ffmpegError < 0)
        return nxcip::NX_UNSUPPORTED_CODEC;

    AVCodecContext* context = encoder->codecContext();

    auto nxSampleType =
        ffmpeg::utils::toNxSampleType(context->sample_fmt);
    if (!nxSampleType.has_value())
        return nxcip::NX_UNSUPPORTED_CODEC;

    audioFormat->compressionType = ffmpeg::utils::toNxCompressionType(context->codec_id);
    audioFormat->sampleRate = context->sample_rate;
    audioFormat->bitrate = context->bit_rate;
#ifdef _WIN32
    audioFormat->byteOrder = nxcip::AudioFormat::boLittleEndian;
#else
    audioFormat->byteOrder = __BYTE_ORDER == BIG_ENDIAN
        ? nxcip::AudioFormat::boBigEndian
        : nxcip::AudioFormat::boLittleEndian;
#endif
    audioFormat->channels = context->channels;
    audioFormat->sampleFmt = nxSampleType.value();
    audioFormat->channelLayout = context->channel_layout;
    audioFormat->blockAlign = context->block_align;
    audioFormat->bitsPerCodedSample = context->bits_per_coded_sample;

    return nxcip::NX_NO_ERROR;
}

void MediaEncoder::fillResolutionList(
    const std::vector<device::video::ResolutionData>& list,
    nxcip::ResolutionInfo* outInfoList,
    int* outInfoListCount) const
{
    const auto getMaxFps = [](
        const std::vector<device::video::ResolutionData>& list,
        int startIndex,
        int width, int height) -> int {
        int maxFps = 0;
        for (int i = startIndex; i < list.size(); ++i)
        {
            const device::video::ResolutionData& resolution = list[i];
            if (resolution.width * resolution.height == width * height)
            {
                if (maxFps < resolution.fps)
                    maxFps = resolution.fps;
            }
        }
        return maxFps;
    };

    int j = 0;
    device::video::ResolutionData previous(0, 0, 0);
    for (int i = 0; i < list.size() && j < nxcip::MAX_RESOLUTION_LIST_SIZE; ++i)
    {
        if (previous.width * previous.height == list[i].width * list[i].height)
            continue;

        outInfoList[j].resolution = { list[i].width, list[i].height };
        outInfoList[j].maxFps = getMaxFps(list, i, list[i].width, list[i].height);
        previous = list[i];
        ++j;
    }
    *outInfoListCount = j;
}

} // namespace nx
} // namespace usb_cam
