#include "camera_manager.h"

#include <cstring>

#include <nx/utils/url.h>

#include "utils/debug.h"
#include "device/utils.h"
#include "native_media_encoder.h"
#include "transcode_media_encoder.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec_parameters.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/error.h"

namespace nx {
namespace rpi_cam2 {

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

int ENCODER_COUNT = 2;

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
    m_encoders.reserve(ENCODER_COUNT);
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
    debug("getEncoder(): %d\n", encoderIndex);
    if(!m_ffmpegStreamReader)
    {
        m_ffmpegStreamReader = std::make_shared<nx::ffmpeg::StreamReader>(
            decodeCameraInfoUrl().c_str(),
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
                    this,
                    m_timeProvider,
                    getEncoderDefaults(encoderIndex),
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
                    this,
                    m_timeProvider,
                    getEncoderDefaults(encoderIndex),
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
    for(const auto & encoder : m_encoders)
        if(encoder)
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
    std::string error = ffmpeg::error::toString(ffmpeg::error::lastError());
    int size = error.size() >= nxcip::MAX_TEXT_LEN ? nxcip::MAX_TEXT_LEN - 1 : error.size();
    for(int i = 0; i < size; ++i)
        errorString[i] = error[i];
    errorString[size] = '\0';
    debug("getLastError(): %s\n", errorString);
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

ffmpeg::CodecParameters CameraManager::getEncoderDefaults(int encoderIndex)
{
    float defaultFPS = 0;
    int defaultBitrate = 0;
    nxcip::Resolution resolution = {0, 0};

    std::string url = decodeCameraInfoUrl();
    auto codecList = device::utils::getSupportedCodecs(url.c_str());
    nxcip::CompressionType nxCodecID = getPriorityCodec(codecList);

    switch (encoderIndex)
    {
        case 0: // native stream
        {
            auto resolutionList = device::utils::getResolutionList(url.c_str(), nxCodecID);
            auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
                [](const device::ResolutionData& a, const device::ResolutionData& b)
                {
                    return a.width * a.height < b.width * b.height;
                });

            if(it != resolutionList.end())
            {
               defaultFPS = it->maxFps;
               defaultBitrate = 2000000;
               resolution = {it->width, it->height};
            }
            break;
        }
        case 1: // motion stream
            defaultFPS = 30;
            defaultBitrate = 200000;
            resolution = {480, 270};
            break;
    }

    return ffmpeg::CodecParameters(
        ffmpeg::utils::toAVCodecID(nxCodecID),
        defaultFPS,
        defaultBitrate,
        resolution.width,
        resolution.height);
}

} // namespace nx
} // namespace rpi_cam2