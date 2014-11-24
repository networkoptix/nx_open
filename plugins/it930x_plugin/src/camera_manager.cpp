#include <string>
#include <sstream>
#include <chrono>
#include <atomic>

#include "it930x.h"
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
        ITE_PARAM_CHANNEL,
        ITE_PARAM_PRESENT,
        ITE_PARAM_STRENGTH,
        // ...
        ITE_PARAM_REBOOT,
        ITE_PARAM_SET_DEFAULTS
    } CameraParams;
#if 0
    const char * PARAM_NAME_CHANNEL =       "/Transmission/Channel";
    const char * PARAM_NAME_PRESENT =       "/Transmission/Present";
    const char * PARAM_NAME_STRENGTH =      "/Transmission/Strength";
    const char * PARAM_NAME_REBOOT =        "/Commands/Reboot";
    const char * PARAM_NAME_SET_DEFAULTS =  "/Commands/SetDefaults";
#endif
    const char * PARAM_QUERY_CHANNEL =       "channel";
    const char * PARAM_QUERY_PRESENT =       "present";
    const char * PARAM_QUERY_STRENGTH =      "strength";
    const char * PARAM_QUERY_REBOOT =        "reboot";
    const char * PARAM_QUERY_SET_DEFAULTS =  "setDefaults";

    const char * PARAM_CHANNEL_0 = "0 (177)";
    const char * PARAM_CHANNEL_1 = "1 (189)";
    const char * PARAM_CHANNEL_2 = "2 (201)";
    const char * PARAM_CHANNEL_3 = "3 (213)";
    const char * PARAM_CHANNEL_4 = "4 (225)";
    const char * PARAM_CHANNEL_5 = "5 (237)";
    const char * PARAM_CHANNEL_6 = "6 (249)";
    const char * PARAM_CHANNEL_7 = "7 (261)";
    const char * PARAM_CHANNEL_8 = "8 (273)";
    const char * PARAM_CHANNEL_9 = "9 (363)";
    const char * PARAM_CHANNEL_10 = "10 (375)";
    const char * PARAM_CHANNEL_11 = "11 (387)";
    const char * PARAM_CHANNEL_12 = "12 (399)";
    const char * PARAM_CHANNEL_13 = "13 (411)";
    const char * PARAM_CHANNEL_14 = "14 (423)";
    const char * PARAM_CHANNEL_15 = "15 (473)";

    unsigned str2num(std::string& s)
    {
        unsigned value;
        std::stringstream ss;
        ss << s;
        ss >> value;
        return value;
    }

    std::string num2str(unsigned id)
    {
        std::stringstream ss;
        ss << id;
        return ss.str();
    }

    CameraParams str2param(const char * paramName)
    {
        if (! strcmp(paramName, PARAM_QUERY_CHANNEL))
            return ITE_PARAM_CHANNEL;
        if (! strcmp(paramName, PARAM_QUERY_PRESENT))
            return ITE_PARAM_PRESENT;
        if (! strcmp(paramName, PARAM_QUERY_STRENGTH))
            return ITE_PARAM_STRENGTH;

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
        size_t start_pos = 0;
        while ( (start_pos = str.find(from, start_pos)) != std::string::npos )
        {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
            ++count;
        }

        return count;
    }

    unsigned replaceSubstring(std::string& str, const char * from, const char * to)
    {
        std::string sFrom(from);
        std::string sTo(to);

        return replaceSubstring(str, sFrom, sTo);
    }
}

namespace ite
{
    RxDevice::RxDevice(unsigned id)
    :   m_id(id),
        m_camera(nullptr),
        m_frequency(0),
        m_strength(0),
        m_present(false)
    {}

    bool RxDevice::open()
    {
        try
        {
            m_devStream.reset();
            m_device.reset(); // dtor first
            m_device.reset(new It930x(m_id));
            return true;
        }
        catch(const char * msg)
        {}

        return false;
    }

    bool RxDevice::lockF(CameraManager * cam, unsigned freq)
    {
        try
        {
            if (isOpen())
                unlockF();
            else
                open();

            m_device->lockChannel(freq);
            m_devStream.reset( new It930Stream( *m_device ) );
            m_frequency = freq;
            m_camera = cam;
            return true;
        }
        catch (const char * msg)
        {}

        return false;
    }

    bool RxDevice::stats()
    {
        try
        {
            bool locked = false;
            uint8_t abortCount = 0;
            float ber = 0.0f;

            m_present = false;
            m_strength = 0;
            m_device->statistic(locked, m_present, m_strength, abortCount, ber);
            return true;
        }
        catch (const char * msg)
        {}

        return false;
    }

    void RxDevice::driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const
    {
        if (m_device)
        {
            char verDriver[32] = "";
            char verAPI[32] = "";
            char verFWLink[32] = "";
            char verFWOFDM[32] = "";
            char xCompany[32] = "";
            char xModel[32] = "";

            m_device->info(verDriver, verAPI, verFWLink, verFWOFDM, xCompany, xModel);

            driverVersion = "Driver: " + std::string(verDriver) + " API: " + std::string(verAPI);
            fwVersion = "FW: " + std::string(verFWLink) + " OFDM: " + std::string(verFWOFDM);
            company = std::string(xCompany);
            model = std::string(xModel);
        }
    }

    unsigned RxDevice::dev2id(const std::string& nm)
    {
        std::string name = nm;
        name.erase( name.find( DEVICE_PATTERN ), strlen( DEVICE_PATTERN ) );

        return str2id(name);
    }

    unsigned RxDevice::str2id(const std::string& name)
    {
        unsigned num;
        std::stringstream ss;
        ss << name;
        ss >> num;
        return num;
    }

    std::string RxDevice::id2str(unsigned id)
    {
        return num2str(id);
    }

    //

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(const nxcip::CameraInfo& info)
    :   m_refManager( DiscoveryManager::refManager() ),
        m_frequency(0),
        m_threadObject( nullptr ),
        m_hasThread( false ),
        m_loading(false),
        m_errorStr( nullptr )
    {
        memcpy( &m_info, &info, sizeof(nxcip::CameraInfo) );

        unsigned short rxID;
        DiscoveryManager::parseInfo(info, m_txID, rxID, m_frequency);

        //tryLoad();
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
            // TODO: lock + check

            paramsXML =
            "<cameras> "
                "<camera name=\"it930x\"> "
                    "<group name=\"Transmission\"> "
                        "<param query=\"{channel}\" name=\"Channel\" dataType=\"Enumeration\" "
                            "min=\"{ch0},{ch1},{ch2},{ch3},{ch4},{ch5},{ch6},{ch7},{ch8},{ch9},{ch10},{ch11},{ch12},{ch13},{ch14},{ch15}\" "
                            "max=\"{ch0},{ch1},{ch2},{ch3},{ch4},{ch5},{ch6},{ch7},{ch8},{ch9},{ch10},{ch11},{ch12},{ch13},{ch14},{ch15}\" /> "
                        "<param query=\"{present}\" name=\"Present\" dataType=\"Bool\" readOnly=\"true\" /> "
                        "<param query=\"{strength}\" name=\"Strength\" dataType=\"MinMaxStep\" readOnly=\"true\" min=\"0\" max=\"100\" /> "
                    "</group> "
                    "<group name=\"Commands\"> "
                        "<param query=\"{reboot}\"  name=\"Reboot\" dataType=\"Button\" /> "
                        "<param query=\"{setDefs}\" name=\"Set Defaults\" dataType=\"Button\" /> "
                    "</group> "
                "</camera> "
            "</cameras>";

            replaceSubstring(paramsXML, "{channel}",    PARAM_QUERY_CHANNEL);
            replaceSubstring(paramsXML, "{present}",    PARAM_QUERY_PRESENT);
            replaceSubstring(paramsXML, "{strength}",   PARAM_QUERY_STRENGTH);
            replaceSubstring(paramsXML, "{reboot}",     PARAM_QUERY_REBOOT);
            replaceSubstring(paramsXML, "{setDefs}",    PARAM_QUERY_SET_DEFAULTS);

            replaceSubstring(paramsXML, "{ch0}", PARAM_CHANNEL_0);
            replaceSubstring(paramsXML, "{ch1}", PARAM_CHANNEL_1);
            replaceSubstring(paramsXML, "{ch2}", PARAM_CHANNEL_2);
            replaceSubstring(paramsXML, "{ch3}", PARAM_CHANNEL_3);
            replaceSubstring(paramsXML, "{ch4}", PARAM_CHANNEL_4);
            replaceSubstring(paramsXML, "{ch5}", PARAM_CHANNEL_5);
            replaceSubstring(paramsXML, "{ch6}", PARAM_CHANNEL_6);
            replaceSubstring(paramsXML, "{ch7}", PARAM_CHANNEL_7);
            replaceSubstring(paramsXML, "{ch8}", PARAM_CHANNEL_8);
            replaceSubstring(paramsXML, "{ch9}", PARAM_CHANNEL_9);
            replaceSubstring(paramsXML, "{ch10}", PARAM_CHANNEL_10);
            replaceSubstring(paramsXML, "{ch11}", PARAM_CHANNEL_11);
            replaceSubstring(paramsXML, "{ch12}", PARAM_CHANNEL_12);
            replaceSubstring(paramsXML, "{ch13}", PARAM_CHANNEL_13);
            replaceSubstring(paramsXML, "{ch14}", PARAM_CHANNEL_14);
            replaceSubstring(paramsXML, "{ch15}", PARAM_CHANNEL_15);
        }

        return paramsXML.c_str();
    }

    int CameraManager::getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const
    {
        std::string strValue;

        switch (str2param(paramName))
        {
            case ITE_PARAM_CHANNEL:
            {
                getParamStr_Channel(strValue);
                break;
            }
            case ITE_PARAM_PRESENT:
                getParamStr_Present(strValue);
                break;
            case ITE_PARAM_STRENGTH:
                getParamStr_Strength(strValue);
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
            valueBufSize = 0;

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
                s = num2str(chan);
        }
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

    bool CameraManager::setParam_Channel(std::string& s)
    {
        unsigned chan = str2num(s);

        // TODO: validate chan

        if (stopStreams() && captureAnyRxDevice())
        {
            return DiscoveryManager::instance()->setChannel(m_txID, m_rxDevice->rxID(), chan);
        }
        return false;
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

        if (! m_frequency)
            return STATE_NO_FREQUENCY;

        if (m_supportedRxDevices.empty() || ! m_rxDevice.get())
            return STATE_NO_RECEIVER;

        if (m_rxDevice.get() && ! m_loading && ! m_hasThread)
            return STATE_DEVICE_READY;

        if (m_loading)
            return STATE_STREAM_LOADING;

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
                    DiscoveryManager::updateInfo(m_info, 0xffff, m_frequency);
                    break;
                }
                // no break
            }

            case STATE_DEVICE_READY:
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

        stopDevReader();
        if (initDevReader())
            initEncoders();

        m_loading = false;
    }

    void CameraManager::openStream(unsigned encNo)
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        m_openedStreams.insert(encNo);
    }

    void CameraManager::closeStream(unsigned encNo)
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        m_openedStreams.erase(encNo);
    }

    bool CameraManager::stopStreams()
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        if (m_openedStreams.size())
            return false;

        stopDevReader();
        m_rxDevice.reset(); // still've been locking frequency
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
        if (!m_frequency || m_supportedRxDevices.empty())
            return false;

        if (m_rxDevice.get())
            return captureSameRxDevice();

        for (size_t i = 0; i < m_supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = m_supportedRxDevices[i];
            if (captureFreeRxDevice(dev))
                return true;
        }

        for (size_t i = 0; i < m_supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = m_supportedRxDevices[i];
            if (captureOpenedRxDevice(dev))
                return true;
        }

        return false;
    }

    bool CameraManager::captureSameRxDevice()
    {
        std::lock_guard<std::mutex> lock( m_rxDevice->mutex() ); // LOCK

        if (m_rxDevice->isLocked() && m_rxDevice->frequency() == m_frequency)
            return true;

        if (!m_rxDevice->isOpen())
            m_rxDevice->open();

        m_rxDevice->unlockF();
        m_rxDevice->lockF(this, m_frequency);
        if (m_rxDevice->isLocked())
        {
            m_rxDevice->stats();
            DiscoveryManager::updateInfo(m_info, m_rxDevice->rxID(), m_frequency);
            return true;
        }

        m_rxDevice.reset();
        return false;
    }

    bool CameraManager::captureFreeRxDevice(RxDevicePtr dev)
    {
        if (! dev.get())
            return false;

        std::lock_guard<std::mutex> lock( dev->mutex() ); // LOCK

        if (dev->isLocked()) // device is reading another channel
            return false;

        if (!dev->isOpen())
            dev->open();

        dev->lockF(this, m_frequency);
        if (dev->isLocked())
        {
            dev->stats();
            m_rxDevice = dev; // under lock
            DiscoveryManager::updateInfo(m_info, m_rxDevice->rxID(), m_frequency);
            return true;
        }

        return false;
    }

    bool CameraManager::captureOpenedRxDevice(RxDevicePtr dev)
    {
        std::lock_guard<std::mutex> lock( dev->mutex() ); // LOCK

        CameraManager * cam = dev->camera();

        if (cam->stopStreams())
        {
            dev->unlockF();
            dev->lockF(this, m_frequency);
            if (dev->isLocked())
            {
                dev->stats();
                m_rxDevice = dev; // under lock
                DiscoveryManager::updateInfo(m_info, m_rxDevice->rxID(), m_frequency);
                return true;
            }
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
                // HACK
                for (unsigned i = 0; !m_threadObject && i < 100; ++i)
                    usleep(10000);

                if (m_threadObject)
                {
                    m_threadObject->stop();
                    m_readThread.join();
                }

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
        static const unsigned MAX_READ_ATTEMPTS = 20;
        static const unsigned SLEEP_DURATION_MS = 50;

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
