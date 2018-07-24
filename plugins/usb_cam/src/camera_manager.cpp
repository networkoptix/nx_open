#include "camera_manager.h"

#include <cstring>

#include <nx/utils/url.h>

#include "device/utils.h"
#include "native_media_encoder.h"
#include "transcode_media_encoder.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec_parameters.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static const std::vector<nxcip::CompressionType> videoCodecPriorityList =
{
    nxcip::AV_CODEC_ID_H264
};

nxcip::CompressionType getPriorityCodec(const std::vector<nxcip::CompressionType>& codecList)
{
    for (const auto codecID : videoCodecPriorityList)
    {
        if (std::find(codecList.begin(), codecList.end(), codecID) != codecList.end())
            return codecID;
    }
    return nxcip::AV_CODEC_ID_NONE;
}

AVCodecID getFfmpegCodecID(const char * devicePath)
{
    nxcip::CompressionType nxCodec = 
        getPriorityCodec(device::getSupportedCodecs(devicePath));
    return ffmpeg::utils::toAVCodecID(nxCodec);
}

int constexpr ENCODER_COUNT = 2;

}

CameraManager::CameraManager(
    const nxcip::CameraInfo& info,
    nxpl::TimeProvider *const timeProvider)
:
    m_info( info ),
    m_timeProvider(timeProvider),
    m_refManager( this ),
    m_pluginRef( Plugin::instance() ),
    m_capabilities(
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        nxcip::BaseCameraManager::primaryStreamSoftMotionCapability)
{
    /* adding nullptr so we can check for it in getEncoder() */
    for(int i = 0; i < ENCODER_COUNT; ++i)
        m_encoders.push_back(nullptr);
}

CameraManager::~CameraManager()
{
}

void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager2, sizeof(nxcip::IID_BaseCameraManager2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int CameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int CameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = ENCODER_COUNT;
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if(!m_ffmpegStreamReader)
    {
        std::string url = decodeCameraInfoUrl();
        m_ffmpegStreamReader = std::make_shared<nx::ffmpeg::StreamReader>(
            url.c_str(),
            getEncoderDefaults(0),
            m_timeProvider);
    }

    switch(encoderIndex)
    {
        case 0:
        {
            if (!m_encoders[encoderIndex])
            {
                m_encoders[encoderIndex].reset(new NativeMediaEncoder(
                    encoderIndex,
                    getEncoderDefaults(encoderIndex),
                    this,
                    m_timeProvider,
                    m_ffmpegStreamReader));
            }
            break;
        }
        case 1:
        {
            if(!m_encoders[encoderIndex])
            {
                m_encoders[encoderIndex].reset(new TranscodeMediaEncoder(
                    encoderIndex,
                    getEncoderDefaults(encoderIndex),
                    this,
                    m_timeProvider,
                    m_ffmpegStreamReader));
            }
            break;
        }
        default:
            return nxcip::NX_INVALID_ENCODER_NUMBER;
    }

    m_encoders[encoderIndex]->addRef();
    *encoderPtr = m_encoders[encoderIndex].get();

    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    memcpy( info, &m_info, sizeof(m_info) );
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

void CameraManager::setCredentials( const char* username, const char* password )
{
    strncpy( m_info.defaultLogin, username, sizeof(m_info.defaultLogin)-1 );
    strncpy( m_info.defaultPassword, password, sizeof(m_info.defaultPassword)-1 );
}

int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
{
    return nxcip::NX_NO_ERROR;
}

nxcip::CameraPtzManager* CameraManager::getPtzManager() const
{
    return NULL;
}

nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

void CameraManager::getLastErrorString( char* errorString ) const
{
    const auto errorToString = 
        [&errorString](int ffmpegError) -> bool
        {
            bool error = ffmpegError < 0;
            if(error)
            {
                std::string s = ffmpeg::utils::errorToString(ffmpegError);
                strncpy(errorString, s.c_str(), nxcip::MAX_TEXT_LEN - 1);
            }
            return error;
        };

    if(m_ffmpegStreamReader && errorToString(m_ffmpegStreamReader->lastFfmpegError()))
        return;

    if(m_encoders[1] && errorToString(m_encoders[1]->lastFfmpegError())
        return;

    *errorString = "\0";
}

int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int CameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

const nxcip::CameraInfo& CameraManager::info() const
{
    return m_info;
}

nxpt::CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}

std::string CameraManager::decodeCameraInfoUrl() const
{
    QString url = QString(m_info.url).mid(9);
    return nx::utils::Url::fromPercentEncoding(url.toLatin1()).toStdString();
}

ffmpeg::CodecParameters CameraManager::getEncoderDefaults(int encoderIndex) const
{
    std::string url = decodeCameraInfoUrl();
    auto nxCodecList = device::getSupportedCodecs(url.c_str());
    
    nxcip::CompressionType nxCodecID = getPriorityCodec(nxCodecList);
    AVCodecID ffmpegCodecID = ffmpeg::utils::toAVCodecID(nxCodecID);

    if (encoderIndex == 0)
    {
        auto resolutionList = device::getResolutionList(url.c_str(), nxCodecID);
        auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
            [](const device::ResolutionData& a, const device::ResolutionData& b)
            {
                return a.width * a.height < b.width * b.height;
            });

        if(it != resolutionList.end())
            return ffmpeg::CodecParameters(ffmpegCodecID, it->maxFps, 2000000, it->width, it->height);
    }
    else if(encoderIndex == 1)
    {
        return ffmpeg::CodecParameters(ffmpegCodecID, 7, 2000000, 480, 270);
    }

    return ffmpeg::CodecParameters(ffmpegCodecID, 0, 0, 0, 0);
}

} // namespace nx
} // namespace usb_cam