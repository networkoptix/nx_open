#include <string>
#include <chrono>
#include <atomic>
#include <ctime>

#include "camera_manager.h"
#include "media_encoder.h"

#include "video_packet.h"
#include "dev_reader.h"

#include "discovery_manager.h"

namespace
{
    typedef enum
    {
        ITE_PARAM_NONE,
        //
        ITE_PARAM_CHANNEL,
        ITE_PARAM_PRESENT,
        ITE_PARAM_STRENGTH,
        ITE_PARAM_QUALITY,
        //
        ITE_PARAM_RX_ID,
        ITE_PARAM_RX_COMPANY,
        ITE_PARAM_RX_MODEL,
        ITE_PARAM_RX_DRIVER_VER,
        ITE_PARAM_RX_API_VER,
        ITE_PARAM_RX_FW_VER,
        //
        ITE_PARAM_TX_ID,
        ITE_PARAM_TX_HWID,
        ITE_PARAM_TX_COMPANY,
        ITE_PARAM_TX_MODEL,
        ITE_PARAM_TX_SERIAL,
        ITE_PARAM_TX_FW_VER,
        //
        ITE_PARAM_REBOOT,
        ITE_PARAM_SET_DEFAULTS
    } CameraParams;

    const char * PARAM_SIGNAL_CHANNEL =         "channel";
    const char * PARAM_SIGNAL_PRESENT =         "present";
    const char * PARAM_SIGNAL_STRENGTH =        "strength";
    const char * PARAM_SIGNAL_QUALITY =         "quality";

    const char * PARAM_RX_ID =                  "rxID";
    const char * PARAM_RX_COMPANY =             "rxCompany";
    const char * PARAM_RX_MODEL =               "rxModel";
    const char * PARAM_RX_DRIVER_VER =          "rxDriverVer";
    const char * PARAM_RX_API_VER =             "rxAPIVer";
    const char * PARAM_RX_FW_VER =              "rxFWVer";

    const char * PARAM_TX_ID =                  "txID";
    const char * PARAM_TX_HWID =                "txHwID";
    const char * PARAM_TX_COMPANY =             "txCompany";
    const char * PARAM_TX_MODEL =               "txModel";
    const char * PARAM_TX_SERIAL =              "txSerial";
    const char * PARAM_TX_FW_VER =              "txFWVer";

    const char * PARAM_COMMAND_REBOOT =         "reboot";
    const char * PARAM_COMMAND_SET_DEFAULTS =   "setDefaults";

    const char * PARAM_CHANNELS[] = {
        "0 (177/6 MHz)",
        "1 (189/6 MHz)",
        "2 (201/6 MHz)",
        "3 (213/6 MHz)",
        "4 (225/6 MHz)",
        "5 (237/6 MHz)",
        "6 (249/6 MHz)",
        "7 (261/6 MHz)",
        "8 (273/6 MHz)",
        "9 (363/6 MHz)",
        "10 (375/6 MHz)",
        "11 (387/6 MHz)",
        "12 (399/6 MHz)",
        "13 (411/6 MHz)",
        "14 (423/6 MHz)",
        "15 (473/6 MHz)"
    };

    CameraParams str2param(const char * paramName)
    {
        if (! strcmp(paramName, PARAM_SIGNAL_CHANNEL))
            return ITE_PARAM_CHANNEL;
        if (! strcmp(paramName, PARAM_SIGNAL_PRESENT))
            return ITE_PARAM_PRESENT;
        if (! strcmp(paramName, PARAM_SIGNAL_STRENGTH))
            return ITE_PARAM_STRENGTH;
        if (! strcmp(paramName, PARAM_SIGNAL_QUALITY))
            return ITE_PARAM_QUALITY;

        if (! strcmp(paramName, PARAM_RX_ID))
            return ITE_PARAM_RX_ID;
        if (! strcmp(paramName, PARAM_RX_COMPANY))
            return ITE_PARAM_RX_COMPANY;
        if (! strcmp(paramName, PARAM_RX_MODEL))
            return ITE_PARAM_RX_MODEL;
        if (! strcmp(paramName, PARAM_RX_DRIVER_VER))
            return ITE_PARAM_RX_DRIVER_VER;
        if (! strcmp(paramName, PARAM_RX_API_VER))
            return ITE_PARAM_RX_API_VER;
        if (! strcmp(paramName, PARAM_RX_FW_VER))
            return ITE_PARAM_RX_FW_VER;

        if (! strcmp(paramName, PARAM_TX_ID))
            return ITE_PARAM_TX_ID;
        if (! strcmp(paramName, PARAM_TX_HWID))
            return ITE_PARAM_TX_HWID;
        if (! strcmp(paramName, PARAM_TX_COMPANY))
            return ITE_PARAM_TX_COMPANY;
        if (! strcmp(paramName, PARAM_TX_MODEL))
            return ITE_PARAM_TX_MODEL;
        if (! strcmp(paramName, PARAM_TX_SERIAL))
            return ITE_PARAM_TX_SERIAL;
        if (! strcmp(paramName, PARAM_TX_FW_VER))
            return ITE_PARAM_TX_FW_VER;

        if (! strcmp(paramName, PARAM_COMMAND_REBOOT))
            return ITE_PARAM_REBOOT;
        if (! strcmp(paramName, PARAM_COMMAND_SET_DEFAULTS))
            return ITE_PARAM_SET_DEFAULTS;

        return ITE_PARAM_NONE;
    }

    unsigned replaceSubstring(std::string& str, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return 0;

        unsigned count = 0;
        size_t pos = 0;
        while ( (pos = str.find(from, 0)) != std::string::npos )
        {
            str.replace(pos, from.length(), to);
            ++count;
        }

        return count;
    }

    unsigned replaceSubstring(std::string& str, const char * from, const char * to)
    {
        if (!from || !to)
            return 0;

        std::string sFrom(from);
        std::string sTo(to);

        return replaceSubstring(str, sFrom, sTo);
    }
}

namespace ite
{
    //

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(const nxcip::CameraInfo& info, const DeviceMapper * devMapper)
    :   m_refManager( DiscoveryManager::refManager() ),
        m_devMapper(devMapper),
        m_loading(false),
        m_errorStr( nullptr )
    {
        updateCameraInfo(info);

        unsigned frequency;
        std::vector<unsigned short> rxIDs;
        DeviceMapper::parseInfo(info, m_txID, frequency, rxIDs);
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

    const char * CameraManager::getParametersDescriptionXML() const
    {
        static std::string paramsXML;

        if (! paramsXML.size()) // first time
        {
            std::string chanEnum;
            chanEnum += PARAM_CHANNELS[0]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[1]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[2]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[3]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[4]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[5]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[6]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[7]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[8]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[9]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[10]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[11]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[12]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[13]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[14]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[15];

            std::string params =
            "<?xml version=\"1.0\"?> "
            "<plugin "
                "name = \"IT930X\" "
                "version = \"1\" "
                "unique_id = \"11761fbb-04f4-40c8-8213-52d9367676c6\"> "
                "<parameters>"
                    "<group name=\"Signal\"> "
                        "<param id=\"{channel}\" name=\"Channel\" dataType=\"Enumeration\" range=\"{chEnum}\" /> "
                        "<param id=\"{present}\" name=\"Signal Presence\" dataType=\"Bool\" readOnly=\"true\" /> "
                        "<param id=\"{strength}\" name=\"Signal Strength\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                        "<param id=\"{quality}\" name=\"Signal Quality\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                    "</group> "
                    "<group name=\"Receiver\"> "
                        "<param id=\"{rxID}\" name=\"Receiver ID\" dataType=\"Number\" readOnly=\"true\" range=\"0,255\" /> "
                        "<param id=\"{rxCompany}\" name=\"Company\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxModel}\" name=\"Model\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxDriverVer}\" name=\"Driver Version\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxAPIVer}\" name=\"API Version\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxFWVer}\" name=\"Firmware Version\" dataType=\"String\" readOnly=\"true\" /> "
                    "</group> "
#if 0
                    "<group name=\"Camera\"> "
                        "<param id=\"{txID}\" name=\"Camera ID\" dataType=\"Number\" readOnly=\"true\" range=\"0,65535\" /> "
                        "<param id=\"{txHwID}\" name=\"Camera HWID\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txCompany}\" name=\"Company\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txModel}\" name=\"Model\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txSerial}\" name=\"Serial Number\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txFWVer}\" name=\"Firmware Version\" dataType=\"String\" readOnly=\"true\" /> "
                    "</group> "
#endif
#if 0
                    "<group name=\"Commands\"> "
                        "<param id=\"{reboot}\"  name=\"Reboot\" dataType=\"Button\" /> "
                        "<param id=\"{setDefs}\" name=\"Set Defaults\" dataType=\"Button\" /> "
                    "</group> "
#endif
                "</parameters> "
            "</plugin>";

            replaceSubstring(params, "{channel}",   PARAM_SIGNAL_CHANNEL);
            replaceSubstring(params, "{present}",   PARAM_SIGNAL_PRESENT);
            replaceSubstring(params, "{strength}",  PARAM_SIGNAL_STRENGTH);
            replaceSubstring(params, "{quality}",   PARAM_SIGNAL_QUALITY);

            replaceSubstring(params, "{rxID}",          PARAM_RX_ID);
            replaceSubstring(params, "{rxCompany}",     PARAM_RX_COMPANY);
            replaceSubstring(params, "{rxModel}",       PARAM_RX_MODEL);
            replaceSubstring(params, "{rxDriverVer}",   PARAM_RX_DRIVER_VER);
            replaceSubstring(params, "{rxAPIVer}",      PARAM_RX_API_VER);
            replaceSubstring(params, "{rxFWVer}",       PARAM_RX_FW_VER);

            replaceSubstring(params, "{txID}",          PARAM_TX_ID);
            replaceSubstring(params, "{txHwID}",        PARAM_TX_HWID);
            replaceSubstring(params, "{txCompany}",     PARAM_TX_COMPANY);
            replaceSubstring(params, "{txModel}",       PARAM_TX_MODEL);
            replaceSubstring(params, "{txSerial}",      PARAM_TX_SERIAL);
            replaceSubstring(params, "{txFWVer}",       PARAM_TX_FW_VER);

            replaceSubstring(params, "{reboot}",    PARAM_COMMAND_REBOOT);
            replaceSubstring(params, "{setDefs}",   PARAM_COMMAND_SET_DEFAULTS);

            replaceSubstring(params, std::string("{chEnum}"), chanEnum);

            // TODO: thread safety
            if (! paramsXML.size())
                paramsXML.swap(params);
        }

        return paramsXML.c_str();
    }

    int CameraManager::getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const
    {
        std::string strValue;

        switch (str2param(paramName))
        {
            // Signal

            case ITE_PARAM_CHANNEL:
                getParamStr_Channel(strValue);
                break;
            case ITE_PARAM_PRESENT:
                getParamStr_Present(strValue);
                break;
            case ITE_PARAM_STRENGTH:
                getParamStr_Strength(strValue);
                break;
            case ITE_PARAM_QUALITY:
                getParamStr_Quality(strValue);
                break;

            // Receiver

            case ITE_PARAM_RX_ID:
                getParamStr_RxID(strValue);
                break;
            case ITE_PARAM_RX_COMPANY:
                getParamStr_RxCompany(strValue);
                break;
            case ITE_PARAM_RX_MODEL:
                getParamStr_RxModel(strValue);
                break;
            case ITE_PARAM_RX_DRIVER_VER:
                getParamStr_RxDriverVer(strValue);
                break;
            case ITE_PARAM_RX_API_VER:
                getParamStr_RxAPIVer(strValue);
                break;
            case ITE_PARAM_RX_FW_VER:
                getParamStr_RxFwVer(strValue);
                break;

            // Camera

            case ITE_PARAM_TX_ID:
                strValue = num2str(m_txID);
                break;
            case ITE_PARAM_TX_HWID:
                getParamStr_TxHwID(strValue);
                break;
            case ITE_PARAM_TX_COMPANY:
                getParamStr_TxCompany(strValue);
                break;
            case ITE_PARAM_TX_MODEL:
                getParamStr_TxModel(strValue);
                break;
            case ITE_PARAM_TX_SERIAL:
                getParamStr_TxSerial(strValue);
                break;
            case ITE_PARAM_TX_FW_VER:
                getParamStr_TxFwVer(strValue);
                break;

            // Command

            case ITE_PARAM_REBOOT:
            case ITE_PARAM_SET_DEFAULTS:
                break;

            case ITE_PARAM_NONE:
            default:
                return nxcip::NX_UNKNOWN_PARAMETER;
        }

        if (strValue.size())
        {
            int strSize = strValue.size();
            if (*valueBufSize < strSize)
            {
                *valueBufSize = strSize;
                return nxcip::NX_MORE_DATA;
            }

            *valueBufSize = strSize;
            strncpy(valueBuf, strValue.c_str(), strSize);
            valueBuf[strSize] = '\0';
        }
        else
        {
            valueBufSize = 0;
            return nxcip::NX_NO_DATA;
        }

        return nxcip::NX_NO_ERROR;
    }

    // TODO
    int CameraManager::setParamValue(const char * paramName, const char * value)
    {
        if (!value)
            return nxcip::NX_INVALID_PARAM_VALUE;

        switch (str2param(paramName))
        {
            case ITE_PARAM_CHANNEL:
            {
                std::string str(value);
                if (! setParam_Channel(str))
                    return nxcip::NX_INVALID_PARAM_VALUE;
                break;
            }

            case ITE_PARAM_PRESENT:
            case ITE_PARAM_STRENGTH:
            case ITE_PARAM_QUALITY:
            //
            case ITE_PARAM_RX_ID:
            case ITE_PARAM_RX_COMPANY:
            case ITE_PARAM_RX_MODEL:
            case ITE_PARAM_RX_DRIVER_VER:
            case ITE_PARAM_RX_API_VER:
            case ITE_PARAM_RX_FW_VER:
            //
            case ITE_PARAM_TX_ID:
            case ITE_PARAM_TX_HWID:
            case ITE_PARAM_TX_COMPANY:
            case ITE_PARAM_TX_MODEL:
            case ITE_PARAM_TX_SERIAL:
            case ITE_PARAM_TX_FW_VER:
                return nxcip::NX_PARAM_READ_ONLY;

            case ITE_PARAM_REBOOT:
                break;
            case ITE_PARAM_SET_DEFAULTS:
                break;

            case ITE_PARAM_NONE:
            default:
                return nxcip::NX_UNKNOWN_PARAMETER;
        }

        return nxcip::NX_NO_ERROR;
    }

    void CameraManager::getParamStr_Channel(std::string& s) const
    {
        if (m_rxDevice && m_rxDevice->frequency())
        {
            unsigned chan = TxDevice::freqChannel(m_rxDevice->frequency());
            if (chan < TxDevice::CHANNELS_NUM)
                s = PARAM_CHANNELS[chan];
        }
    }

    void CameraManager::getParamStr_RxID(std::string& s) const
    {
        if (m_rxDevice)
            s = num2str(m_rxDevice->rxID());
    }

    void CameraManager::getParamStr_Present(std::string& s) const
    {
        if (m_rxDevice)
        {
            if (m_rxDevice->present())
                s = "true";
            else
                s = "false";
        }
    }

    void CameraManager::getParamStr_Strength(std::string& s) const
    {
        if (m_rxDevice)
            s = num2str(m_rxDevice->strength());
    }

    void CameraManager::getParamStr_Quality(std::string& s) const
    {
        if (m_rxDevice)
            s = num2str(m_rxDevice->quality());
    }

    bool CameraManager::setParam_Channel(std::string& s)
    {
        /// @warning need value in front of string
        unsigned chan = str2num(s);
        if (chan >= TxDevice::CHANNELS_NUM)
            return false;

        if (m_rxDevice)
        {
            //std::lock_guard<std::mutex> lock( m_rxDevice->mutex() ); // LOCK device

            RxDevicePtr dev = m_rxDevice;
            unsigned curFreq = m_rxDevice->frequency();

            /// @warning locks m_reloadMutex inside (possible deadlock if we lock device)
            if (! stopStreams(true))
                return false;

            if (curFreq && dev->tryLockF(curFreq))
                return dev->setChannel(m_txID, chan);
        }

        return false;
    }

    //

    int CameraManager::getEncoderCount( int* encoderCount ) const
    {
#if 0
        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_FREQUENCY:
            case STATE_NO_RECEIVER:
            case STATE_DEVICE_READY:
            case STATE_STREAM_LOADING:
            case STATE_STREAM_LOST:
                *encoderCount = 0;
                return nxcip::NX_TRY_AGAIN;

            case STATE_STREAM_READY:
            case STATE_STREAM_READING:
                break;
        }
#endif
        *encoderCount = RxDevice::streamsCount();
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        tryLoad();

        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_FREQUENCY:
            case STATE_NO_RECEIVER:
            case STATE_DEVICE_READY:
            case STATE_STREAM_LOADING:
            case STATE_STREAM_LOST:
                return nxcip::NX_TRY_AGAIN;

            case STATE_STREAM_READY:
            case STATE_STREAM_READING:
                break;
        }

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (m_encoders.empty())
            return nxcip::NX_TRY_AGAIN;

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
        *capabilitiesMask =
                nxcip::BaseCameraManager::nativeMediaStreamCapability |
                nxcip::BaseCameraManager::primaryStreamSoftMotionCapability;
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
        if (! m_txID)
            return STATE_NO_CAMERA;

        if (! m_rxDevice)
            return STATE_NO_RECEIVER;

        if (! m_rxDevice->frequency())
            return STATE_NO_FREQUENCY;

        if (m_rxDevice && ! m_loading && ! m_rxDevice->isLocked())
            return STATE_DEVICE_READY;

        if (m_loading)
            return STATE_STREAM_LOADING;

        if (m_rxDevice->isLocked() && ! m_rxDevice->isReading())
            return STATE_STREAM_LOST;

        if (m_rxDevice->isReading() && m_openedStreams.empty())
            return STATE_STREAM_READY;

        return STATE_STREAM_READING;
    }

    void CameraManager::tryLoad()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_FREQUENCY:
                return; // can do nothing

            case STATE_NO_RECEIVER:
            {
                bool ok = captureAnyRxDevice();
                if (! ok)
                    break;
                // no break
            }

            case STATE_DEVICE_READY:
            case STATE_STREAM_LOST:
            {
                reloadMedia();
                break;
            }

            case STATE_STREAM_LOADING:
            case STATE_STREAM_READY:
            case STATE_STREAM_READING:
                break;
        }
    }

    void CameraManager::reloadMedia()
    {
        m_loading = true;

        stopEncoders();
        if (m_rxDevice && m_rxDevice->isLocked())
        {
            initEncoders();
            if (! m_rxDevice->isReading())
                freeDevice();
        }

        m_loading = false;
    }

    bool CameraManager::stopIfNeedIt()
    {
        static const unsigned WAIT_TO_STOP_MS = 30000; // 30 sec

        if (m_stopTimer.isStarted() && m_stopTimer.elapsed() > WAIT_TO_STOP_MS)
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
            if (m_rxDevice->isLocked() && m_rxDevice->txID() == m_txID)
            {
                m_stopTimer.stop();
                return true;
            }
            //else
            freeDevice();
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

        unsigned freq = m_devMapper->getFreq4Tx(m_txID);
        if (freq == 0)
            return best;

        std::vector<RxDevicePtr> supportedRxDevices;
        m_devMapper->getRx4Tx(m_txID, supportedRxDevices);

        for (size_t i = 0; i < supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = supportedRxDevices[i];

            bool isGood = true;
            if (dev->lockCamera(m_txID, freq)) // delay inside
            {
                isGood = dev->good();
                if (isGood)
                {
                    if (!best || dev->strength() > best->strength())
                        best.swap(dev);
                }

                // unlock all but best
                if (dev)
                    dev->unlockF();
            }
        }

        return best;
    }

    void CameraManager::freeDevice()
    {
        if (m_rxDevice)
        {
            m_rxDevice->unlockF();
            m_rxDevice.reset();
        }
        m_stopTimer.stop();
    }

    void CameraManager::initEncoders()
    {
        if (!m_rxDevice || !m_rxDevice->isLocked())
            return;

        unsigned streamsCount = m_rxDevice->streamsCount();
        m_encoders.reserve(streamsCount);

        for (unsigned i = 0; i < streamsCount; ++i)
        {
            nxcip::ResolutionInfo res;
            m_rxDevice->resolution(i, res.resolution.width, res.resolution.height, res.maxFps);

            if (i >= m_encoders.size())
            {
                auto enc = std::shared_ptr<MediaEncoder>( new MediaEncoder(this, i, res), MediaEncoder::fakeFree ); // FIXME
                m_encoders.push_back(enc);
            }
            else
                m_encoders[i]->updateResolution(res);

            m_rxDevice->subscribe(i);
        }
    }

    void CameraManager::stopEncoders()
    {
        for (unsigned i = 0; i < m_rxDevice->streamsCount(); ++i)
            m_rxDevice->unsubscribe(i);
    }

    // from StreamReader thread

    VideoPacket * CameraManager::nextPacket(unsigned encoderNumber)
    {
        static const unsigned MAX_READ_ATTEMPTS = 500;
        static const unsigned SLEEP_DURATION_MS = 20;

        for (unsigned i = 0; i < MAX_READ_ATTEMPTS; ++i)
        {
            DevReader * devReader = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                if (m_rxDevice)
                    devReader = m_rxDevice->reader();
            }

            if (! devReader)
                return nullptr;

            ContentPacketPtr pkt = devReader->getPacket( RxDevice::stream2pid(encoderNumber) );

            if (pkt)
            {
                if (!pkt->size())
                    printf("%s empty packet", __FUNCTION__);

                // TODO: better ContentPacket -> VideoPacket
                VideoPacket * packet = new VideoPacket(pkt->data(), pkt->size());

                packet->setPTS(pkt->pts());

                if (encoderNumber)
                    packet->setLowQualityFlag();
                if (pkt->flags() & ContentPacket::F_StreamReset)
                    packet->setStreamReset();

                return packet;
            }

            Timer::sleep(SLEEP_DURATION_MS);
        }

        return nullptr;
    }

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
        freeDevice();
        return true;
    }
}
