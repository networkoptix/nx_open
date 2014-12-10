#include <string>
#include <chrono>
#include <atomic>

#include "camera_manager.h"
#include "media_encoder.h"

#include "video_packet.h"
#include "libav_wrap.h"
#include "dev_reader.h"

#include "discovery_manager.h"


extern "C"
{
    static int readDevice(void* opaque, uint8_t* buf, int bufSize)
    {
        ite::DevReader * r = reinterpret_cast<ite::DevReader*>(opaque);
        return r->read(buf, bufSize);
    }
}

namespace
{
    typedef enum
    {
        ITE_PARAM_NONE,
        ITE_PARAM_RXID,
        ITE_PARAM_CHANNEL,
        ITE_PARAM_PRESENT,
        ITE_PARAM_STRENGTH,
        ITE_PARAM_QUALITY,
        // ...
        ITE_PARAM_REBOOT,
        ITE_PARAM_SET_DEFAULTS
    } CameraParams;

    const char * PARAM_QUERY_RXID =          "rxID";
    const char * PARAM_QUERY_CHANNEL =       "channel";
    const char * PARAM_QUERY_PRESENT =       "present";
    const char * PARAM_QUERY_STRENGTH =      "strength";
    const char * PARAM_QUERY_QUALITY =       "quality";
    const char * PARAM_QUERY_REBOOT =        "reboot";
    const char * PARAM_QUERY_SET_DEFAULTS =  "setDefaults";

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
        if (! strcmp(paramName, PARAM_QUERY_RXID))
            return ITE_PARAM_RXID;
        if (! strcmp(paramName, PARAM_QUERY_CHANNEL))
            return ITE_PARAM_CHANNEL;
        if (! strcmp(paramName, PARAM_QUERY_PRESENT))
            return ITE_PARAM_PRESENT;
        if (! strcmp(paramName, PARAM_QUERY_STRENGTH))
            return ITE_PARAM_STRENGTH;
        if (! strcmp(paramName, PARAM_QUERY_QUALITY))
            return ITE_PARAM_QUALITY;

        if (! strcmp(paramName, PARAM_QUERY_REBOOT))
            return ITE_PARAM_REBOOT;
        if (! strcmp(paramName, PARAM_QUERY_SET_DEFAULTS))
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

    CameraManager::CameraManager(const nxcip::CameraInfo& info)
    :   m_refManager( DiscoveryManager::refManager() ),
        m_waitStop(false),
        m_stopTime(0),
        m_threadObject( nullptr ),
        m_hasThread( false ),
        m_loading(false),
        m_errorStr( nullptr )
    {
        memcpy( &m_info, &info, sizeof(nxcip::CameraInfo) );

        unsigned short rxID;
        unsigned frequency;
        DiscoveryManager::parseInfo(info, m_txID, rxID, frequency);
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
                        "<param id=\"{rxID}\" name=\"DTV Receiver\" dataType=\"Number\" readOnly=\"true\" range=\"0,256\" /> "
                        "<param id=\"{present}\" name=\"Signal Presence\" dataType=\"Bool\" readOnly=\"true\" /> "
                        "<param id=\"{strength}\" name=\"Signal Strength\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                        "<param id=\"{quality}\" name=\"Signal Quality\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                    "</group> "
#if 0
                    "<group name=\"Commands\"> "
                        "<param id=\"{reboot}\"  name=\"Reboot\" dataType=\"Button\" /> "
                        "<param id=\"{setDefs}\" name=\"Set Defaults\" dataType=\"Button\" /> "
                    "</group> "
#endif
                "</parameters> "
            "</plugin>";

            replaceSubstring(params, "{rxID}",      PARAM_QUERY_RXID);
            replaceSubstring(params, "{channel}",   PARAM_QUERY_CHANNEL);
            replaceSubstring(params, "{present}",   PARAM_QUERY_PRESENT);
            replaceSubstring(params, "{strength}",  PARAM_QUERY_STRENGTH);
            replaceSubstring(params, "{quality}",   PARAM_QUERY_QUALITY);
            replaceSubstring(params, "{reboot}",    PARAM_QUERY_REBOOT);
            replaceSubstring(params, "{setDefs}",   PARAM_QUERY_SET_DEFAULTS);
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
            case ITE_PARAM_RXID:
                getParamStr_RxID(strValue);
                break;
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

            case ITE_PARAM_REBOOT:
                break;
            case ITE_PARAM_SET_DEFAULTS:
                break;

            case ITE_PARAM_NONE:
            default:
                return nxcip::NX_UNKNOWN_PARAMETER;
        }

        if (strValue.size())
        {
            unsigned strSize = strValue.size();
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

            case ITE_PARAM_RXID:
            case ITE_PARAM_PRESENT:
            case ITE_PARAM_STRENGTH:
            case ITE_PARAM_QUALITY:
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
            unsigned chan = DeviceInfo::freqChannel(m_rxDevice->frequency());
            if (chan < DeviceInfo::CHANNELS_NUM)
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
        if (chan >= DeviceInfo::CHANNELS_NUM)
            return false;

        if (m_rxDevice)
        {
            //std::lock_guard<std::mutex> lock( m_rxDevice->mutex() ); // LOCK device

            RxDevicePtr dev = m_rxDevice;
            unsigned freq = m_rxDevice->frequency();

            /// @warning locks m_reloadMutex inside (possible deadlock if we lock device)
            if (! stopStreams(true))
                return false;

            if (freq && dev->lockF(freq))
                return dev->setChannel(m_txID, chan);
        }

        return DiscoveryManager::instance()->setChannel(m_txID, chan);
    }

    //

    int CameraManager::getEncoderCount( int* encoderCount ) const
    {
        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_FREQUENCY:
            case STATE_NO_RECEIVER:
            case STATE_DEVICE_READY:
            case STATE_STREAM_LOADING:
            case STATE_STREAM_LOST:
#if 0
                *encoderCount = 0;
                return nxcip::NX_TRY_AGAIN;
#else
                *encoderCount = 2; // TODO: get from RC
                return nxcip::NX_NO_ERROR;
#endif

            case STATE_STREAM_READY:
            case STATE_STREAM_READING:
                break;
        }

        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

        *encoderCount = m_encoders.size();
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

        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

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

        if (m_supportedRxDevices.empty() || ! m_rxDevice)
            return STATE_NO_RECEIVER;

        if (! m_rxDevice->frequency())
            return STATE_NO_FREQUENCY;

        if (m_rxDevice && ! m_loading && ! m_hasThread)
            return STATE_DEVICE_READY;

        if (m_loading)
            return STATE_STREAM_LOADING;

        if (m_hasThread && !m_threadObject)
            return STATE_STREAM_LOST;

        if (m_hasThread && m_openedStreams.empty())
            return STATE_STREAM_READY;

        return STATE_STREAM_READING;
    }

    void CameraManager::tryLoad()
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

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
                {
                    DiscoveryManager::updateInfo(m_info, 0xffff, 0);
                    break;
                }
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

        // try light reload
        stopDevReader();
        if (initDevReader())
            initEncoders();

        // soft reload failed, change state to hard reload (next time)
        if (! m_hasThread)
            freeDevice();

        m_loading = false;
    }

    void CameraManager::openStream(unsigned encNo)
    {
        m_waitStop = false;

        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        m_openedStreams.insert(encNo);
        m_stopTime = 0;
    }

    void CameraManager::closeStream(unsigned encNo)
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        m_openedStreams.erase(encNo);
        if (m_openedStreams.empty())
            m_waitStop = true;
    }

    bool CameraManager::stopStreams(bool force)
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        if (!m_rxDevice)
            return false;

        if (m_openedStreams.size() && ! force)
            return false;

        stopDevReader();
        freeDevice();
        return true;
    }

    void CameraManager::addRxDevice(RxDevicePtr dev)
    {
        bool found = false;
        for (auto it = m_supportedRxDevices.begin(); it != m_supportedRxDevices.end(); ++it)
        {
            if ((*it)->rxID() == dev->rxID()) {
                found = true;
                break;
            }
        }

        if (!found)
            m_supportedRxDevices.push_back(dev);
    }

    bool CameraManager::captureAnyRxDevice()
    {
        if (m_supportedRxDevices.empty())
            return false;

        if (m_rxDevice)
        {
            if (captureSameRxDevice(m_rxDevice))
                return true;
            //else
            freeDevice();
        }

        for (size_t i = 0; i < m_supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = m_supportedRxDevices[i];
            if (captureSameRxDevice(dev))
                return true;
        }

        for (size_t i = 0; i < m_supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = m_supportedRxDevices[i];
            if (captureFreeRxDevice(dev))
                return true;
        }

        return false;
    }

    bool CameraManager::captureSameRxDevice(RxDevicePtr dev)
    {
        if (! dev.get())
            return false;

        std::lock_guard<std::mutex> lock( dev->mutex() ); // LOCK

        if (dev->isLocked() && dev->txID() == m_txID)
        {
            if (dev != m_rxDevice)
                m_rxDevice = dev;
            return true;
        }

        if (!dev->isOpen())
            dev->open();

        dev->unlockF();
        dev->lockCamera(m_txID);
        if (dev->isLocked())
        {
            if (dev != m_rxDevice)
                m_rxDevice = dev;
            m_rxDevice->updateDevParams();
            DiscoveryManager::updateInfo(m_info, m_rxDevice->rxID(), m_rxDevice->frequency());
            return true;
        }

        return false;
    }

    bool CameraManager::captureFreeRxDevice(RxDevicePtr dev)
    {
        if (! dev.get())
            return false;

        std::lock_guard<std::mutex> lock( dev->mutex() ); // LOCK

        if (dev->isLocked()) // device is busy
            return false;

        if (!dev->isOpen())
            dev->open();

        dev->lockCamera(m_txID);
        if (dev->isLocked())
        {
            m_rxDevice = dev; // under lock
            m_rxDevice->updateDevParams();
            DiscoveryManager::updateInfo(m_info, m_rxDevice->rxID(), m_rxDevice->frequency());
            return true;
        }

        return false;
    }

    bool CameraManager::initDevReader()
    {
        if (!m_rxDevice || !m_rxDevice->isLocked())
            return false;

        static const unsigned DEFAULT_FRAME_SIZE = It930x::DEFAULT_PACKETS_NUM * It930x::MPEG_TS_PACKET_SIZE;

        m_devReader.reset( new DevReader( m_rxDevice->devStream(), DEFAULT_FRAME_SIZE ) );
        return true;
    }

    void CameraManager::stopDevReader()
    {
        stopEncoders();
        m_devReader.reset();
    }

    void CameraManager::initEncoders()
    {
        stopEncoders();

        static const float HARDCODED_FPS = 30.0f;
        static const unsigned WAIT_LIBAV_INIT_DURATION_S = 10;
        static std::chrono::seconds dura( WAIT_LIBAV_INIT_DURATION_S );

        m_devReader->clear();
        m_devReader->startTimer( dura );

        auto up = std::unique_ptr<LibAV>( new LibAV(m_devReader.get(), readDevice) );

        if (up && up->isOK() && m_devReader->timerAlive())
        {
            m_devReader->stopTimer();

            m_libAV.reset( up.release() );
            unsigned streamsCount = m_libAV->streamsCount();

            if (streamsCount)
            {
                std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

                unsigned minPoints = 0;
                unsigned lowResId = 0;
                m_encoders.reserve( streamsCount );
                for (unsigned i=0; i < streamsCount; ++i)
                {
                    auto enc = std::shared_ptr<MediaEncoder>( new MediaEncoder(this, i), MediaEncoder::fakeFree );
                    m_encoders.push_back( enc );
                    nxcip::ResolutionInfo& res = enc->resolution();
                    res.maxFps = HARDCODED_FPS; // m_libAV->fps( i );
                    m_libAV->resolution( i, res.resolution.width, res.resolution.height );

                    unsigned points = res.resolution.width * res.resolution.height;
                    if (!i || points < minPoints)
                    {
                        minPoints = points;
                        lowResId = i;
                    }
                }

                m_encQueues.reserve( m_encoders.size() );
                for (size_t i=0; i < m_encoders.size(); ++i)
                    m_encQueues.push_back( std::make_shared<VideoPacketQueue>() );

                if (m_encQueues.size())
                    m_encQueues[lowResId]->setLowQuality();
            }

            if (streamsCount)
            {
                try
                {
                    m_readThread = std::thread( ReadThread(this, m_libAV.get()) );
                    m_hasThread = true;
                }
                catch (std::system_error& )
                {}
            }
        }
    }

    void CameraManager::stopEncoders()
    {
        try
        {
            if (m_hasThread)
            {
                /// @warning HACK: if m_threadObject creates later join leads to deadlock.
                /// Resource reload mutex could not lock. Camera can't reload itself.
                for (unsigned i = 0; !m_threadObject && i < 100; ++i)
                    usleep(10000);

                if (m_threadObject)
                    m_threadObject->stop();

                m_readThread.join(); // possible deadlock here
                m_hasThread = false;
            }

            m_threadObject = nullptr;
        }
        catch (const std::exception& e)
        {
            //std::string s = e.what();
        }

        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK
        m_encoders.clear();
        m_encQueues.clear();

        // Only one m_libAV copy here
        if (m_libAV)
            m_libAV.reset();
    }

    std::shared_ptr<VideoPacketQueue> CameraManager::queue(unsigned num) const
    {
        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

        if (m_encQueues.size() <= num)
            return std::shared_ptr<VideoPacketQueue>();

        return m_encQueues[num];
    }

    // from StreamReader thread
    VideoPacket* CameraManager::nextPacket(unsigned encoderNumber)
    {
        static const unsigned MAX_READ_ATTEMPTS = 50;
        static const unsigned SLEEP_DURATION_MS = 20;

        static std::chrono::milliseconds dura( SLEEP_DURATION_MS );

        std::shared_ptr<VideoPacketQueue> wantedQueue = queue( encoderNumber );
        if (!wantedQueue)
            return nullptr;

        for (unsigned i = 0; i < MAX_READ_ATTEMPTS; ++i)
        {
            {
                std::lock_guard<std::mutex> lock( wantedQueue->mutex() ); // LOCK

                if (wantedQueue->size())
                {
                    std::unique_ptr<VideoPacket> packet( new VideoPacket() );
                    wantedQueue->front()->swap( *packet );
                    wantedQueue->pop_front();

                    if (wantedQueue->isLowQuality())
                        packet->setLowQualityFlag();
                    return packet.release();
                }
            }

            std::this_thread::sleep_for(dura);
        }

        return nullptr;
    }
#if 0
    void CameraManager::fillInfo(nxcip::CameraInfo& info) const
    {
        std::string driver, firmware, company, model;
        m_rxDevice->driverInfo( driver, firmware, company, model );

        driver += " ";
        driver += firmware;

        unsigned modelSize = std::min( model.size(), sizeof(info.modelName)-1 );
        unsigned fwareSize = std::min( driver.size(), sizeof(info.firmware)-1 );

        strncpy( info.modelName, model.c_str(), modelSize );
        strncpy( info.firmware, driver.c_str(), fwareSize );

        info.modelName[modelSize] = '\0';
        info.firmware[fwareSize] = '\0';
    }
#endif
}
