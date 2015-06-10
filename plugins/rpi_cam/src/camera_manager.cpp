#include <string>
#include <cstdlib>

#include "rpi_camera.h"

#include "discovery_manager.h"
#include "media_encoder.h"
#include "camera_manager.h"

namespace
{
    void refDeleter(nxpl::PluginInterface * ptr)
    {
        if( ptr )
            ptr->releaseRef();
    }
}

namespace rpi_cam
{
    static unsigned ENCODERS_COUNT = 2;

    DEFAULT_REF_COUNTER(nxcip::BaseCameraManager)

    CameraManager::CameraManager()
    :   DefaultRefCounter( DiscoveryManager::refManager() )
    {
        debug_print("%s\n", __FUNCTION__);

        m_rpiCamera = std::make_shared<RPiCamera>(m_parameters);
        if (m_rpiCamera)
        {
            m_encoderHQ = std::shared_ptr<MediaEncoder>( new MediaEncoder(m_rpiCamera, 0), refDeleter );
            m_encoderLQ = std::shared_ptr<MediaEncoder>( new MediaEncoder(m_rpiCamera, 1), refDeleter );
        }

        makeInfo();
    }

    CameraManager::~CameraManager()
    {
        debug_print("%s\n", __FUNCTION__);
    }

    void CameraManager::makeInfo()
    {
        static const char * str = "PiCam";
        unsigned strLen = strlen(str);

        static const char * id = RPiCamera::UID();
        unsigned idLen = strlen(id);

        memset( &m_info, 0, sizeof(nxcip::CameraInfo) );
        strncpy( m_info.url, str, std::min(strLen, sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( m_info.uid, id, std::min(idLen, sizeof(nxcip::CameraInfo::uid)-1) );
        strncpy( m_info.modelName, str, std::min(strLen, sizeof(nxcip::CameraInfo::modelName)-1) );
    }

    void * CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_BaseCameraManager)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager*>(this);
        }

        return nullptr;
    }

    int CameraManager::getEncoderCount(int * encoderCount) const
    {
        debug_print("%s\n", __FUNCTION__);
        *encoderCount = ENCODERS_COUNT;
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder(int encoderIndex, nxcip::CameraMediaEncoder ** encoderPtr)
    {
        //debug_print("%s\n", __FUNCTION__);

        if (static_cast<unsigned>(encoderIndex) >= ENCODERS_COUNT)
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        if (encoderIndex)
        {
            if (! m_encoderLQ)
                return nxcip::NX_INVALID_ENCODER_NUMBER;

            m_encoderLQ->addRef();
            *encoderPtr = m_encoderLQ.get();
        }
        else
        {
            if (! m_encoderHQ)
                return nxcip::NX_INVALID_ENCODER_NUMBER;

            m_encoderHQ->addRef();
            *encoderPtr = m_encoderHQ.get();
        }

        //debug_print("%s %d - OK\n", __FUNCTION__, encoderIndex);
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraInfo(nxcip::CameraInfo * info) const
    {
        memcpy( info, &m_info, sizeof(m_info) );
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraCapabilities(unsigned int * capabilitiesMask) const
    {
        *capabilitiesMask = nxcip::BaseCameraManager::nativeMediaStreamCapability;
        return nxcip::NX_NO_ERROR;
    }

    void CameraManager::setCredentials(const char *, const char * )
    {
    }

    int CameraManager::setAudioEnabled(int )
    {
        return nxcip::NX_NO_ERROR;
    }

    nxcip::CameraPtzManager* CameraManager::getPtzManager() const
    {
        return nullptr;
    }

    nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
    {
        return nullptr;
    }

    nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
    {
        return nullptr;
    }

    void CameraManager::getLastErrorString(char * errorString) const
    {
        using namespace rpi_omx;

        if (errorString)
        {
            const char * str = err2str((OMX_ERRORTYPE)OMXExeption::lastError());
            if (str)
                strncpy( errorString, str, nxcip::MAX_TEXT_LEN );
        }
    }
}
