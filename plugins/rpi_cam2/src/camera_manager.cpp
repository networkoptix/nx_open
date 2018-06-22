#include "camera_manager.h"

#include <cstring>

#include <nx/utils/url.h>

#include "native_media_encoder.h"
#include "transcode_media_encoder.h"
#include "ffmpeg/stream_reader.h"
#include "utils/utils.h"

namespace nx {
namespace webcam_plugin {

namespace {

static const std::vector<nxcip::CompressionType> videoCodecPriorityList =
{
    nxcip::AV_CODEC_ID_H264,
    nxcip::AV_CODEC_ID_MJPEG
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
    debug("%s\n", __FUNCTION__);

    int encoderCount = 2;
    m_encoders.reserve(encoderCount);
    m_encoders.push_back(nullptr);
    m_encoders.push_back(nullptr);
    
    debug("%s done\n", __FUNCTION__);
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
    debug("%s\n", __FUNCTION__);
    *encoderCount = m_encoders.size();
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    debug("%s\n", __FUNCTION__);

    if(!m_ffmpegStreamReader)
    {
        m_ffmpegStreamReader = std::make_shared<ffmpeg::StreamReader>(
            decodeCameraInfoUrl().c_str(), 
            getEncoderDefaults(0));
    }

    switch(encoderIndex)
    {
        case 0:
        {
            if (!m_encoders[0].get())
            {
                m_encoders[0].reset(new NativeMediaEncoder(
                    this,
                    m_timeProvider,
                    getEncoderDefaults(0),
                    m_ffmpegStreamReader));
            }
            break;
        }
        case 1:
        {
            if(!m_encoders[1].get())
            {
                m_encoders[1].reset(new TranscodeMediaEncoder(
                    this,
                    m_timeProvider,
                    getEncoderDefaults(1),
                    m_ffmpegStreamReader));
            }
            break;
        }
        default:
            return nxcip::NX_INVALID_ENCODER_NUMBER;
    }

    // if (!m_encoder.get())
    // {
    //     m_encoder.reset(new MediaEncoder(
    //         this,
    //         m_timeProvider,
    //         encoderIndex,
    //         getEncoderDefaults()));
    // }
    // m_encoder->addRef();

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
    for(const auto & encoder : m_encoders)
        encoder->updateCameraInfo( m_info );
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
    errorString[0] = '\0';
    //TODO/IMPL
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

CodecContext CameraManager::getEncoderDefaults(int encoderIndex)
{
    debug("%s\n", __FUNCTION__);

    float defaultFPS = 30;
    int defaultBitrate = 200000; 
    nxcip::Resolution resolution;

    std::string url = decodeCameraInfoUrl();
    auto codecList = utils::getSupportedCodecs(url.c_str());
    nxcip::CompressionType codecID = getPriorityCodec(codecList);

    switch (encoderIndex)
    {
        case 0: // native stream
        {
            auto resolutionList = utils::getResolutionList(url.c_str(), codecID);
            auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
                [](const utils::ResolutionData& a, const utils::ResolutionData& b)
                {
                    return
                        a.resolution.width
                        * a.resolution.height <
                        b.resolution.width
                        * b.resolution.height;
                });

            bool valid = it != resolutionList.end();

            defaultFPS = valid ? it->maxFps : 30;
            defaultBitrate = valid ? (int) it->bitrate : 200000;
            resolution = valid ? it->resolution : nxcip::Resolution();
            break;
        }
        case 1: // motion stream
            defaultFPS = 30;
            defaultBitrate = 200000;
            resolution = {480, 270};
            break;
        default:
        return CodecContext();
    }

    return CodecContext(codecID, resolution, defaultFPS, defaultBitrate);
}

} // namespace nx
} // namespace webcam_plugin