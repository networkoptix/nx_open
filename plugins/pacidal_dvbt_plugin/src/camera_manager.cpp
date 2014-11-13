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

namespace ite
{
    RxDevice::RxDevice(unsigned id)
    :   m_id(id)
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

    bool RxDevice::lockF(unsigned freq)
    {
        try
        {
            if (isOpen())
                unlockF();
            else
                open();

            m_device->lockChannel(freq);
            m_devStream.reset( new It930Stream( *m_device ) );
            return true;
        }
        catch (const char * msg)
        {}

        return false;
    }

    bool RxDevice::stats()
    {
        static const unsigned MIN_SIGNAL_STRENGTH = 50; // 0..100
        //static const unsigned MAX_SIGNAL_ABORTS = 1000; // 0..10000 for this device version
        //static const unsigned MAX_SIGNAL_BER = 50;

        bool locked = false, presented = false;
        uint8_t abortCount = 0;
        float ber = 0.0f;

        try
        {
            m_device->statistic( locked, presented, m_strength, abortCount, ber );
        }
        catch (const char * msg)
        {
            //m_errorStr = "Can't get statistics";
            return false;
        }

        // Detecting NO SIGNAL
        if (m_strength < MIN_SIGNAL_STRENGTH)
            return false;
#if 0
        if (abortCount > MAX_SIGNAL_ABORTS || ber > MAX_SIGNAL_BER)
            return false;
#endif
        return true;
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
        std::stringstream ss;
        ss << id;
        return ss.str();
    }

    //

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(const nxcip::CameraInfo& info)
    :   m_refManager( DiscoveryManager::refManager() ),
        m_threadObject( nullptr ),
        m_hasThread( false ),
        m_threadJoined( true ),
        m_errorStr( nullptr )
    {
        memcpy( &m_info, &info, sizeof(nxcip::CameraInfo) );

        m_txID = RxDevice::str2id( info.uid );
    }

    CameraManager::~CameraManager()
    {
        m_rxDevice->close(); // keep it
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

    const char * CameraManager::getParametersDescriptionXML() const
    {
        static const char * paramsXML =
            "<cameras> \
               <camera name=\"ite_cam\"> \
                   <group name=\"main\"> \
                       <param name=\"Frequency\" dataType=\"Enumeration\" readOnly=\"true\" \
                            min=\"177000,201000\" \
                            max=\"177000,201000\" \
                        /> \
                    </group> \
               </camera> \
            </cameras>";
        return paramsXML;
    }

    int CameraManager::getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const
    {
        if (strcmp(paramName, "/main/it930x_freq") == 0)
        {
            static const char * FREQ = "177000";

            unsigned strSize = strlen(FREQ) + 1;
            if( *valueBufSize < strSize )
            {
                *valueBufSize = strSize;
                return nxcip::NX_MORE_DATA;
            }

            *valueBufSize = strSize;
            strncpy(valueBuf, FREQ, strSize);
            return nxcip::NX_NO_ERROR;
        }

        return nxcip::NX_UNKNOWN_PARAMETER;
    }

    int CameraManager::setParamValue(const char * paramName, const char * value)
    {
        if (strcmp(paramName, "/main/it930x_freq") == 0)
            return nxcip::NX_NO_ERROR;

        return nxcip::NX_UNKNOWN_PARAMETER;
    }

    int CameraManager::getEncoderCount( int* encoderCount ) const
    {
#if 0
        if (!hasEncoders())
        {
            *encoderCount = 0;
            return nxcip::NX_TRY_AGAIN;
        }

        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

        *encoderCount = m_encoders.size();
#else
        *encoderCount = 2; // FIXME
#endif
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

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

    void CameraManager::reload()
    {
        std::lock_guard<std::mutex> lock( m_reloadMutex ); // LOCK

        // check reload from another thread
        if (hasEncoders())
            return;

#define RELOAD_VARIVANT 2
#if RELOAD_VARIVANT == 1
        stopDevReader();
        m_rxDevice->stopDevice();
        if (m_rxDevice->lockFrequency() && m_rxDevice->stats() && initDevReader())
            initEncoders();
#elif RELOAD_VARIVANT == 2
        m_rxDevice->stats();
        stopDevReader();
        if (initDevReader())
            initEncoders();
#elif RELOAD_VARIVANT == 3
        stopEncoders();
        initEncoders();
#endif
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
                m_readThread = std::thread( ReadThread(this, m_libAV.get()) );
                //m_hasThread = true;
                m_threadJoined = false;
            }
        }
    }

    void CameraManager::stopEncoders()
    {
        try
        {
            if (m_threadObject)
                m_threadObject->stop();

            if (!m_threadJoined) {
                m_readThread.join();
                m_threadJoined = true;
            }

            m_threadObject = nullptr;
        }
        catch (const std::exception& e)
        {
            std::string s = e.what();
        }

        std::lock_guard<std::mutex> lock( m_encMutex ); // LOCK

        for (auto it = m_encoders.begin(); it != m_encoders.end(); ++it)
            (*it)->releaseRef();
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
