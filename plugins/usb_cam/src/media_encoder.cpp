#include "media_encoder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/url.h>
#include <nx/utils/app_info.h>

#include "camera_manager.h"
#include "stream_reader.h"
#include "utils.h"
#include "device/utils.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/stream_reader.h"

namespace nx {
namespace usb_cam {

MediaEncoder::MediaEncoder(
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    CameraManager* const cameraManager,
    nxpl::TimeProvider *const timeProvider,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    m_encoderIndex(encoderIndex),
    m_codecParams(codecParams),
    m_refManager(cameraManager->refManager()),
    m_cameraManager(cameraManager),
    m_timeProvider(timeProvider),
    m_info(m_cameraManager->info()),
    m_ffmpegStreamReader(ffmpegStreamReader)
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
    strcpy( urlBuf, m_info.url );
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

    std::string url = utils::decodeCameraInfoUrl(m_info.url);
    auto codecDescriptorList = device::getSupportedCodecs(url.c_str());
    auto descriptor = utils::getPriorityDescriptor(codecDescriptorList);

    //NX_ASSERT(descriptor);
    if (descriptor)
    {
        std::vector<device::ResolutionData> list = 
            device::getResolutionList(url.c_str(), descriptor);

        std::sort(list.begin(), list.end(), 
            [](const device::ResolutionData& a, const device::ResolutionData& b)
            {
                return a.width * a.height < b.width * b.height;
            });

        NX_DEBUG(this) << "getResolutionList()::m_info.modelName:" << m_info.modelName;
        int i, j;
        device::ResolutionData previous(0,0,0);
        for (i = 0, j = 0; i < list.size() && j < nxcip::MAX_RESOLUTION_LIST_SIZE; ++i)
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
    std::string url = utils::decodeCameraInfoUrl(m_info.url);
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
    std::string url = utils::decodeCameraInfoUrl(m_info.url);
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
    static_cast<void>( audioFormat );
    return nxcip::NX_UNSUPPORTED_CODEC;
}

void MediaEncoder::updateCameraInfo(const nxcip::CameraInfo& info)
{
    m_info = info;
}

int MediaEncoder::lastFfmpegError() const
{
    return m_streamReader ? m_streamReader->lastFfmpegError() : 0;
}

} // namespace nx
} // namespace usb_cam