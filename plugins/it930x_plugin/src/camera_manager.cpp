#include <string>
#include <chrono>
#include <atomic>
#include <ctime>

#include "camera_manager.h"
#include "media_encoder.h"

#include "discovery_manager.h"
#include "advanced_settings.h"

namespace
{
    void refDeleter(nxpl::PluginInterface * ptr)
    {
        if( ptr )
            ptr->releaseRef();
    }
}

namespace ite
{
    CameraManager::CameraManager(const RxDevicePtr &rxDev)
    :   m_rxDevice(rxDev),
        m_errorStr(nullptr),
        m_cameraId(rxDev->getTxDevice()->txID())
    {
        m_rxDevice->setCamera(this);
        m_info = m_rxDevice->getCameraInfo();
    }

    void * CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_BaseCameraManager3)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager3*>(this);
        }
        if (interfaceID == nxcip::IID_BaseCameraManager2)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager2*>(this);
        }
        if (interfaceID == nxcip::IID_BaseCameraManager)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager*>(this);
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    //

    const char * CameraManager::getParametersDescriptionXML() const
    {
        return AdvancedSettings::getDescriptionXML();
    }

    int CameraManager::getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const
    {
        return AdvancedSettings(m_rxDevice->getTxDevice(), m_rxDevice).getParamValue(paramName, valueBuf, valueBufSize);
    }

    int CameraManager::setParamValue(const char * paramName, const char * value)
    {
        return AdvancedSettings(m_rxDevice->getTxDevice(), m_rxDevice).setParamValue(*this, paramName, value);
    }

    void CameraManager::needUpdate(unsigned group)
    {
        m_update[group] = true;
    }

    void CameraManager::updateSettings()
    {
        if (!m_rxDevice)
            return;

        if (m_update[AdvancedSettings::GROUP_OSD])
        {
            m_update[AdvancedSettings::GROUP_OSD] = false;

            m_rxDevice->updateOSD();
        }

        if (m_update[AdvancedSettings::GROUP_SIGNAL])
        {
            m_update[AdvancedSettings::GROUP_SIGNAL] = false;

            if (m_newChannel == m_rxDevice->channel())
                return;

            if (m_rxDevice->changeChannel(m_newChannel))
            {
                //stopStreams(true);
                //m_devMapper->forgetTx(txID());
            }
        }
    }

    //

    int CameraManager::getEncoderCount(int * encoderCount) const
    {
        *encoderCount = RxDevice::streamsCountPassive();
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder(int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_rxDevice || !rxDeviceRef()->deviceReady())
        {
            ITE_LOG() << FMT("[getEnCoder] No rxDevice");
            return nxcip::NX_NO_DATA;
        }

        if (static_cast<unsigned>(encoderIndex) >= RxDevice::streamsCountPassive())
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        *encoderPtr = new MediaEncoder(this, encoderIndex);
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        memcpy( info, &m_info, sizeof(m_info) );
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        *capabilitiesMask = nxcip::BaseCameraManager::nativeMediaStreamCapability |
                            nxcip::BaseCameraManager::needIFrameDetectionCapability |
                            nxcip::BaseCameraManager::shareIpCapability;
        return nxcip::NX_NO_ERROR;
    }

    void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
    {
    }

    int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
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

    void CameraManager::getLastErrorString( char* errorString ) const
    {
        if (errorString)
        {
            errorString[0] = '\0';
            if (m_errorStr)
                strncpy( errorString, m_errorStr, nxcip::MAX_TEXT_LEN );
        }
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

    // internals

    // from StreamReader thread
    void CameraManager::openStream(unsigned encNo)
    {
        m_stopTimer.stop();
        m_openedStreams.insert(encNo);
    }

    void CameraManager::closeStream(unsigned encNo)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        m_openedStreams.erase(encNo);
        if (m_openedStreams.empty())
            m_stopTimer.restart();
    }

    // from another thread

    bool CameraManager::stopStreams(bool force)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (!m_rxDevice)
            return false;

        if (m_openedStreams.size() && ! force)
            return false;

        return true;
    }
}
