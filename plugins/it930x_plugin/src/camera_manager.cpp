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
    INIT_OBJECT_COUNTER(CameraManager)
    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(const nxcip::CameraInfo& info, DeviceMapper * devMapper, TxDevicePtr txDev)
    :   m_refManager(this),
        m_devMapper(devMapper),
        m_txDevice(txDev),
        m_errorStr(nullptr)
    {
        updateCameraInfo(info);

        unsigned frequency;
        unsigned short txID;
        std::vector<unsigned short> rxIDs;
        DeviceMapper::parseInfo(info, txID, frequency, rxIDs);
    }

    CameraManager::~CameraManager()
    {}

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
        return AdvancedSettings(m_txDevice, m_rxDevice).getParamValue(paramName, valueBuf, valueBufSize);
    }

    int CameraManager::setParamValue(const char * paramName, const char * value)
    {
        return AdvancedSettings(m_txDevice, m_rxDevice).setParamValue(*this, paramName, value);
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
                stopStreams(true);
                m_devMapper->forgetTx(txID());
            }
        }
    }

    //

    int CameraManager::getEncoderCount(int * encoderCount) const
    {
        *encoderCount = RxDevice::streamsCountPassive();
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        State state = tryLoad();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_RECEIVER:
            case STATE_NOT_LOCKED:
            case STATE_NO_ENCODERS:
                return nxcip::NX_TRY_AGAIN;

            case STATE_READY:
            case STATE_READING:
                break;
        }

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        //if (m_encoders.empty())
        //    return nxcip::NX_TRY_AGAIN;

        if (static_cast<unsigned>(encoderIndex) >= m_encoders.size())
            return nxcip::NX_INVALID_ENCODER_NUMBER;

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
        *capabilitiesMask = nxcip::BaseCameraManager::nativeMediaStreamCapability |
                            nxcip::BaseCameraManager::needIFrameDetectionCapability;
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

    CameraManager::State CameraManager::checkState() const
    {
        if (! m_txDevice)
            return STATE_NO_CAMERA;

        if (! m_rxDevice)
            return STATE_NO_RECEIVER;

        if (! m_rxDevice->isLocked() || ! m_rxDevice->isReading())
            return STATE_NOT_LOCKED;

        if (m_rxDevice->isLocked() && m_rxDevice->isReading() && m_encoders.size() == 0)
            return STATE_NO_ENCODERS;

        if (m_openedStreams.empty())
            return STATE_READY;

        return STATE_READING;
    }

    CameraManager::State CameraManager::tryLoad()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
                return state; // can do nothing

            case STATE_NO_RECEIVER:
            case STATE_NOT_LOCKED:
            {
                if (! captureAnyRxDevice())
                    break;
                //break;
            }

            case STATE_NO_ENCODERS:
            {
                reloadMedia();
                break;
            }

            case STATE_READY:
            case STATE_READING:
                return state;
        }

        return checkState();
    }

    void CameraManager::reloadMedia()
    {
        stopEncoders();
        if (m_rxDevice && m_rxDevice->isLocked())
        {
            initEncoders();
            if (! m_rxDevice->isReading())
                freeRx();
        }
    }

    bool CameraManager::stopIfNeedIt()
    {
        static const unsigned WAIT_TO_STOP_MS = 10000;

        if (m_stopTimer.isStarted() && m_stopTimer.elapsedMS() > WAIT_TO_STOP_MS)
            return stopStreams();

        return false;
    }

    void CameraManager::updateCameraInfo(const nxcip::CameraInfo& info)
    {
        memcpy(&m_info, &info, sizeof(nxcip::CameraInfo));
    }

    bool CameraManager::captureAnyRxDevice()
    {
        // reopen same device
        if (m_rxDevice)
        {
            if (m_rxDevice->isLocked() && m_rxDevice->txID() == txID())
            {
                m_stopTimer.stop();
                return true;
            }
            //else
            freeRx();
        }

        RxDevicePtr dev = captureFreeRxDevice();

        if (dev)
        {
            m_rxDevice = dev;
            return true;
        }

        return false;
    }

    RxDevicePtr CameraManager::captureFreeRxDevice()
    {
        RxDevicePtr best;

        std::vector<RxDevicePtr> supportedRxDevices;
        m_devMapper->getRx4Tx(txID(), supportedRxDevices);

        for (size_t i = 0; i < supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = supportedRxDevices[i];

            bool isGood = true;
            if (dev->lockCamera(m_txDevice)) // delay inside
            {
                isGood = dev->good();
                if (isGood)
                {
                    if (!best || dev->strength() > best->strength())
                        best.swap(dev);
                }

                // unlock all but best
                if (dev)
                    dev->unlockC();
            }
            else
                debug_printf("[camera] can't lock Rx for Tx: %d\n", txID());
        }

        if (supportedRxDevices.empty())
            debug_printf("[camera] no Rx for Tx: %d\n", txID());

        return best;
    }

    void CameraManager::freeRx(bool resetRx)
    {
        if (m_rxDevice)
        {
            m_rxDevice->unlockC(resetRx);
            m_rxDevice.reset();
        }
        m_stopTimer.stop();
    }

    void CameraManager::initEncoders()
    {
        if (!m_rxDevice || !m_rxDevice->isLocked())
            return;

        unsigned streamsCount = m_rxDevice->streamsCount();
        if (! streamsCount)
            streamsCount = 2; // HACK

        m_encoders.reserve(streamsCount);
        for (unsigned i = 0; i < streamsCount; ++i)
        {
            if (i >= m_encoders.size())
            {
                auto enc = std::shared_ptr<MediaEncoder>( new MediaEncoder(this, i), refDeleter );
                m_encoders.push_back(enc);
            }

            m_rxDevice->subscribe(i);
        }
    }

    void CameraManager::stopEncoders()
    {
        for (unsigned i = 0; i < m_rxDevice->streamsCount(); ++i)
            m_rxDevice->unsubscribe(i);
    }

    // from StreamReader thread

    void CameraManager::openStream(unsigned encNo)
    {
        m_stopTimer.stop();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

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

        stopEncoders();
        freeRx();
        return true;
    }
}
