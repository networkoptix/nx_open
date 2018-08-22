#include "media_encoder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/url.h>
#include <nx/utils/app_info.h>

#include "camera_manager.h"
#include "stream_reader.h"
#include "utils.h"
#include "device/utils.h"
#include "ffmpeg/default_audio_encoder.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/codec.h"

namespace nx {
namespace usb_cam {

MediaEncoder::MediaEncoder(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    m_refManager(parentRefManager),
    m_encoderIndex(encoderIndex),
    m_codecParams(codecParams),
    m_camera(camera)
{
}

MediaEncoder::~MediaEncoder()
{
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder2, sizeof(nxcip::IID_CameraMediaEncoder2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int MediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int MediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl( char* urlBuf ) const
{
    strcpy( urlBuf, m_camera->info().url );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
{
    const auto getMaxFps = 
        [](
            const std::vector<device::ResolutionData>& list,
            int startIndex, 
            int width, 
            int height) -> int
        {
            int maxFps = 0;
            for (int i = startIndex; i < list.size(); ++i)
            {
                const device::ResolutionData& resolution = list[i];
                if (resolution.width * resolution.height == width * height)
                {
                    if (maxFps < resolution.fps)
                        maxFps = resolution.fps;
                }
            }
            return maxFps;
        };

    std::string url = utils::decodeCameraInfoUrl(m_camera->info().url);
    auto codecDescriptorList = device::getSupportedCodecs(url.c_str());
    auto descriptor = utils::getPriorityDescriptor(codecDescriptorList);

    if (descriptor)
    {
        std::vector<device::ResolutionData> list = 
            device::getResolutionList(url.c_str(), descriptor);

        std::sort(list.begin(), list.end(), 
            [](const device::ResolutionData& a, const device::ResolutionData& b)
            {
                return a.width * a.height < b.width * b.height;
            });

        NX_DEBUG(this) << "getResolutionList()::m_camera->info().modelName:" << m_camera->info().modelName;
        int j = 0;
        device::ResolutionData previous(0, 0, 0);
        for (int i = 0; i < list.size() && j < nxcip::MAX_RESOLUTION_LIST_SIZE; ++i)
        {
            if(previous.width * previous.height == list[i].width * list[i].height)
                continue;

            NX_DEBUG(this) << "w:" << list[i].width << ", h:" << list[i].height;
            infoList[j].resolution = {list[i].width, list[i].height};
            infoList[j].maxFps = getMaxFps(list, i, list[i].width, list[i].height);
            previous = list[i];
            ++j;
        }
        *infoListCount = j;
        return nxcip::NX_NO_ERROR;
    }

    *infoListCount = 0;
    return nxcip::NX_OTHER_ERROR;
}

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    std::string url = utils::decodeCameraInfoUrl(m_camera->info().url);
    nxcip::CompressionType nxCodecID = ffmpeg::utils::toNxCompressionType(m_codecParams.codecID);
    *maxBitrate =  device::getMaxBitrate(url.c_str(), nxCodecID) / 1000;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    m_codecParams.setResolution(resolution.width, resolution.height);
    if (m_streamReader)
        m_streamReader->setResolution(resolution);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
    std::string url = utils::decodeCameraInfoUrl(m_camera->info().url);
    auto descriptor = utils::getPriorityDescriptor(device::getSupportedCodecs(url.c_str()));

    if (!descriptor)
        return nxcip::NX_OTHER_ERROR;

    auto resolutionList = device::getResolutionList(url.c_str(), descriptor);

    std::sort(resolutionList.begin(), resolutionList.end(),
        [&](const device::ResolutionData& a, const device::ResolutionData& b)
        {
            return a.fps < b.fps;
        });

    int size = resolutionList.size();
    int actualFps = 0;
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

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    // the plugin uses bits per second internally, so convert to that first
    int bitratebps = bitrateKbps * 1000;
    m_codecParams.bitrate = bitratebps;
    if (m_streamReader)
        m_streamReader->setBitrate(bitratebps);
    *selectedBitrateKbps = bitrateKbps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
{
    int ffmpegError = 0;
    std::unique_ptr<ffmpeg::Codec> encoder = ffmpeg::getDefaultMp3Encoder(&ffmpegError);
    if(ffmpegError < 0)
        return nxcip::NX_UNSUPPORTED_CODEC;
    
    AVCodecContext * context = encoder->codecContext();

    bool ok;
    nxcip::AudioFormat::SampleType nxSampleType = 
    ffmpeg::utils::toNxSampleType(context->sample_fmt, nullptr, &ok);
    if (!ok)
        return nxcip::NX_UNSUPPORTED_CODEC;

    audioFormat->compressionType = ffmpeg::utils::toNxCompressionType(context->codec_id);
    audioFormat->sampleRate = context->sample_rate;
    audioFormat->bitrate = context->bit_rate;
    audioFormat->byteOrder = __BYTE_ORDER == BIG_ENDIAN 
        ? nxcip::AudioFormat::boBigEndian 
        : nxcip::AudioFormat::boLittleEndian;
    audioFormat->channels = context->channels;
    audioFormat->sampleFmt = nxSampleType;
    audioFormat->channelLayout = context->channel_layout;
    audioFormat->blockAlign = context->block_align;
    audioFormat->bitsPerCodedSample = context->bits_per_coded_sample;

    return nxcip::NX_NO_ERROR;
}

} // namespace nx
} // namespace usb_cam