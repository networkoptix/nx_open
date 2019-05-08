#include "media_encoder.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/vms/utils/installation_info.h>

#include "camera/default_audio_encoder.h"
#include "camera_manager.h"
#include "device/video/utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "stream_reader.h"

namespace nx::usb_cam {

namespace {

static constexpr int kBytesInOneKilobyte = 1000;
static constexpr int kDefaultSecondaryFps = 5;
static constexpr int kDefaultSecondaryWidth = 640;

}

MediaEncoder::MediaEncoder(nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const std::shared_ptr<Camera>& camera)
    :
    m_refManager(parentRefManager),
    m_isSecondaryStream(encoderIndex != 0),
    m_camera(camera)
{
    NX_DEBUG(this, "Create media encoder %1", encoderIndex);
    m_streamReader.reset(new StreamReader(
        &m_refManager,
        m_camera,
        !m_isSecondaryStream));
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
    if (memcmp(&interfaceID, &nxcip::IID_CameraMediaEncoder5,
            sizeof(nxcip::IID_CameraMediaEncoder)) == 0)
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder5*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
{
    m_streamReader->addRef();
    return m_streamReader.get();
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
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    int bitrate = m_camera->videoStream().getMaxBitrate();
    *maxBitrate = bitrate / kBytesInOneKilobyte;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution(const nxcip::Resolution& resolution)
{
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    m_streamReader->setResolution(resolution);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps(const float& fps, float* selectedFps)
{
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    if (m_isSecondaryStream)
    {
        m_streamReader->setFps(fps);
        *selectedFps = fps;
        return nxcip::NX_NO_ERROR;
    }

    auto resolutionList = m_camera->videoStream().resolutionList();
    if (resolutionList.empty())
        return nxcip::NX_OTHER_ERROR;

    std::sort(resolutionList.begin(), resolutionList.end(),
        [&](const device::video::ResolutionData& a, const device::video::ResolutionData& b) {
            return a.fps < b.fps;
        });

    float actualFps = 0;
    for (const auto& resolutionData: resolutionList)
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
    m_streamReader->setFps(actualFps);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate(int bitrateKbps, int* selectedBitrateKbps)
{
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    // the plugin uses bits per second internally, so convert to that first
    int bitrate = bitrateKbps * 1000;
    m_streamReader->setBitrate(bitrate);
    *selectedBitrateKbps = bitrateKbps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getAudioFormat(nxcip::AudioFormat* audioFormat) const
{
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    AVCodecContext* context = m_camera->audioStream().getCodecContext();
    if (context == nullptr)
        return nxcip::NX_UNSUPPORTED_CODEC;

    auto nxSampleType = ffmpeg::utils::toNxSampleType(context->sample_fmt);
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

const char* MediaEncoder::audioExtradata() const
{
    AVCodecContext* context = m_camera->audioStream().getCodecContext();
    if (context == nullptr)
        return nullptr;
    return (char*)context->extradata;
}

int MediaEncoder::audioExtradataSize() const
{
    AVCodecContext* context = m_camera->audioStream().getCodecContext();
    if (context == nullptr)
        return 0;
    return context->extradata_size;
}

int MediaEncoder::getResolutionList(
    nxcip::ResolutionInfo* infoList,
    int* infoListCount) const
{
    if (!m_camera->videoStream().pluggedIn())
        return nxcip::NX_IO_ERROR;

    auto list = m_camera->videoStream().resolutionList();
    if (list.empty())
    {
        *infoListCount = 0;
        return nxcip::NX_IO_ERROR;
    }

    fillResolutionList(list, infoList, infoListCount);
    if (m_isSecondaryStream)
    {
        int index = list.size() < nxcip::MAX_RESOLUTION_LIST_SIZE
            ? (int)list.size()
            : nxcip::MAX_RESOLUTION_LIST_SIZE - 1;

        const auto& resolution = list.back();
        float aspectRatio = (float) resolution.width / resolution.height;
        infoList[index].resolution =
            { kDefaultSecondaryWidth, (int)(kDefaultSecondaryWidth / aspectRatio) };
        infoList[index].maxFps = kDefaultSecondaryFps;

        *infoListCount = index + 1;
        if (nx::vms::utils::installationInfo().hwPlatform ==
            nx::vms::api::HwPlatform::raspberryPi)
        {
            int width = kDefaultSecondaryWidth / 2;
            infoList[0].resolution = { width, (int)(width / aspectRatio) };
            infoList[0].maxFps = kDefaultSecondaryFps;
            *infoListCount = 1;
        }
    }

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
        for (uint32_t i = startIndex; i < list.size(); ++i)
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
    for (uint32_t i = 0; i < list.size() && j < nxcip::MAX_RESOLUTION_LIST_SIZE; ++i)
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

int MediaEncoder::getConfiguredLiveStreamReader(
    nxcip::LiveStreamConfig* config, nxcip::StreamReader** reader)
{
    if (config)
    {
        setResolution({config->width, config->height});
        int selectedBitrate;
        setBitrate(config->bitrateKbps, &selectedBitrate);
        float selectedFps;
        setFps(config->framerate, &selectedFps);
        bool audioEnabled = config->flags & nxcip::LiveStreamConfig::LIVE_STREAM_FLAG_AUDIO_ENABLED;
        m_camera->setAudioEnabled(audioEnabled);
    }
    *reader = getLiveStreamReader();
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setMediaUrl(const char url[nxcip::MAX_TEXT_LEN])
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int MediaEncoder::getVideoFormat(
    nxcip::CompressionType* /*codec*/, nxcip::PixelFormat* /*pixelFormat*/) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

} // namespace nx::usb_cam
