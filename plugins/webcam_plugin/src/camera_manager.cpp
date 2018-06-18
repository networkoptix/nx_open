#include <cstring>

#include "camera_manager.h"
#include "media_encoder.h"
#include "utils.h"

namespace nx {
namespace webcam_plugin {

CameraManager::CameraManager(const nxcip::CameraInfo& info,
                             nxpl::TimeProvider *const timeProvider)
:
    m_refManager( this ),
    m_pluginRef( Plugin::instance() ),
    m_info( info ),
    m_capabilities(
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        nxcip::BaseCameraManager::primaryStreamSoftMotionCapability),
    m_timeProvider(timeProvider)
{
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
    *encoderCount = 1;
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex > 0 )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if (!m_encoder.get())
    {
        m_encoder.reset(new MediaEncoder(
            this,
            m_timeProvider,
            encoderIndex,
            getEncoderDefaults()));
    }
    m_encoder->addRef();

    *encoderPtr = m_encoder.get();

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
    if( m_encoder )
        m_encoder->updateCameraInfo( m_info );
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

CodecContext CameraManager::getEncoderDefaults()
{
    QString url = QString(m_info.url).mid(9);
    url = nx::utils::Url::fromPercentEncoding(url.toLatin1());

    auto codecList = utils::getSupportedCodecs(url.toLatin1().data());
    nxcip::CompressionType codecID = m_videoCodecPriority.getPriorityCodec(codecList);

    auto resolutionList = utils::getResolutionList(url.toLatin1().data(), codecID);
    auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
        [](const utils::ResolutionData& a, const utils::ResolutionData& b)
        {
            return
                a.resolution.width
                 * a.resolution.height <
                b.resolution.width
                 * b.resolution.height;
        });

    static float defaultFPS = 30;
    static int64_t defaultBitrate = 192000;
    auto res = it != resolutionList.end() ? it->resolution : nxcip::Resolution();

    return CodecContext(codecID, res, defaultFPS, defaultBitrate);
}

} // namespace nx
} // namespace webcam_plugin