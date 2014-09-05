#include <string>
#include <sstream>
#include <chrono>

#include "pacidal_it930x.h"
#include "camera_manager.h"
#include "media_encoder.h"

#include "video_packet.h"
#include "libav_wrap.h"

#include "discovery_manager.h"

namespace pacidal
{
    //!
    class DevReader
    {
    public:
        DevReader(PacidalStream* s, size_t size)
        :
            m_stream(s),
            m_size(size),
            m_pos(0)
        {
            readDev();
        }

        int read(uint8_t* buf, int bufSize)
        {
            if (bufSize <= size())
            {
                memcpy(buf, data(), bufSize);
                m_pos += bufSize;
                return bufSize;
            }

            int hasRead = size();
            memcpy(buf, data(), size());
            buf += hasRead;
            bufSize -= hasRead;

            readDev();
            return hasRead + read(buf, bufSize);
        }

    private:
        PacidalStream * m_stream;
        std::vector<uint8_t> m_buf;
        const size_t m_size;
        size_t m_pos;

        const uint8_t* data() const { return &m_buf[m_pos]; }
        int size() const { return m_buf.size() - m_pos; }

        void readDev()
        {
            m_buf.resize(m_size);
            size_t ret = m_stream->read( &m_buf[0], m_size );
            if (ret != m_size)
                m_buf.resize(ret);
            m_pos = 0;
        }
    };

    static int readDevice(void* opaque, uint8_t* buf, int bufSize)
    {
        DevReader * r = reinterpret_cast<DevReader*>(opaque);
        return r->read(buf, bufSize);
    }

    //!
    class ReadThread
    {
    public:
        static const unsigned PACKETS_QUEUE_SIZE = 32*1024;

        ReadThread(CameraManager* camera, LibAV* lib)
        :
            m_camera( camera ),
            m_libAV( lib )
        {
        }

        void operator () ()
        {
            for(;;)
            {
                unsigned streamNum;
                std::unique_ptr<VideoPacket> packet = nextDevPacket( streamNum );
                if (! packet)
                    continue;

                std::shared_ptr<VideoPacket> sp( packet.release() );

                {
                    std::shared_ptr<VideoPacketQueue> queue = m_camera->queue( streamNum );

                    std::lock_guard<std::mutex> lock( queue->mutex() ); // LOCK

                    if( queue->size() >= PACKETS_QUEUE_SIZE )
                        queue->pop_front();
                    queue->push_back(sp);
                }
            }
        }

    private:
        CameraManager* m_camera;
        LibAV* m_libAV;

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum)
        {
            AVPacket avPacket;
            if( m_libAV->nextFrame( &avPacket ) < 0 || avPacket.flags & AV_PKT_FLAG_CORRUPT )
            {
                m_libAV->freePacket( &avPacket );
                return std::unique_ptr<VideoPacket>();
            }

            std::unique_ptr<VideoPacket> packet( new VideoPacket( avPacket.data, avPacket.size ) );
            packet->setTime( avPacket.pts );
            if( avPacket.flags & AV_PKT_FLAG_KEY )
                packet->setKeyFlag();

            streamNum = avPacket.stream_index;
            m_libAV->freePacket( &avPacket );
            return packet;
        }
    };
}

namespace pacidal
{
    static const unsigned DEFAULT_FREQUENCY = 177000;
    static const unsigned MIN_SIGNAL_STRENGTH = 50; // 0..100
    static const unsigned MAX_READ_ATTEMPTS = 10;
    static const unsigned SLEEP_DURATION_MS = 30;

    static const unsigned DEFAULT_FRAME_SIZE = PacidalIt930x::DEFAULT_PACKETS_NUM * PacidalIt930x::MPEG_TS_PACKET_SIZE;

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager( const nxcip::CameraInfo& info, bool test )
    :
        m_refManager( DiscoveryManager::refManager() ),
        m_info( info ),
        m_errorStr(nullptr),
        devInited_(false),
        encInited_(false)
    {
        try
        {
            unsigned id = cameraId( info );
            m_device.reset( new PacidalIt930x( id ) );
        }
        catch (const char * msg)
        {
            m_errorStr = msg;
            return;
        }

        initDevice();
        if( devInited_ && !test )
            initEncoders();
    }

    CameraManager::~CameraManager()
    {
        try
        {
            m_readThread.join();
        }
        catch (...)
        {
        }
    }

    void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if( interfaceID == nxcip::IID_BaseCameraManager2 )
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager2*>(this);
        }
        if( interfaceID == nxcip::IID_BaseCameraManager )
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager*>(this);
        }
        if( interfaceID == nxpl::IID_PluginInterface )
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getEncoderCount
    int CameraManager::getEncoderCount( int* encoderCount ) const
    {
        *encoderCount = m_encoders.size();
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getEncoder
    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        if( !encInited_ || static_cast<unsigned>(encoderIndex) > m_encoders.size() )
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        m_encoders[encoderIndex]->addRef();
        *encoderPtr = m_encoders[encoderIndex].get();
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraInfo
    int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        memcpy( info, &m_info, sizeof(m_info) );
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
    int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        *capabilitiesMask =
                nxcip::BaseCameraManager::nativeMediaStreamCapability |
                nxcip::BaseCameraManager::primaryStreamSoftMotionCapability;
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::setCredentials
    void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
    {
    }

    //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
    int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getPTZManager
    nxcip::CameraPtzManager* CameraManager::getPtzManager() const
    {
        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
    {
        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
    {
        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getLastErrorString
    void CameraManager::getLastErrorString( char* errorString ) const
    {
        if( errorString )
        {
            errorString[0] = '\0';
            if( m_errorStr )
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

    void CameraManager::initDevice()
    {
        m_device->lockChannel( DEFAULT_FREQUENCY );

        bool locked, presented;
        uint8_t quality, strength;
        m_device->statistic(locked, presented, quality, strength);

        // low signal == no data. Don't capture trash.
        if( strength < MIN_SIGNAL_STRENGTH )
            return;

        m_devStream.reset( new PacidalStream(*m_device) ); // after lockChannel()
        m_devReader.reset( new DevReader( m_devStream.get(), DEFAULT_FRAME_SIZE ) );
        devInited_ = true;
    }

    void CameraManager::initEncoders()
    {
        m_libAV.reset(new LibAV(m_devReader.get(), readDevice));

        m_encoders.reserve( m_libAV->streamsCount() );
        for( unsigned i=0; i<m_libAV->streamsCount(); ++i )
            m_encoders.push_back( std::make_shared<MediaEncoder>(this, i) );

        m_encQueues.reserve( m_encoders.size() );
        for(size_t i=0; i < m_encoders.size(); ++i)
            m_encQueues.push_back( std::make_shared<VideoPacketQueue>() );

        ReadThread rt( this, m_libAV.get() );
        m_readThread = std::thread( rt );

        encInited_ = true;
    }

    void CameraManager::resolution( unsigned encoderNum, nxcip::ResolutionInfo& outRes ) const
    {
        outRes.maxFps = m_libAV->fps( encoderNum );
        m_libAV->resolution( encoderNum, outRes.resolution.width, outRes.resolution.height );
    }

    VideoPacket* CameraManager::nextPacket( unsigned encoderNumber )
    {
        std::shared_ptr<VideoPacketQueue> wantedQueue = queue( encoderNumber );

        for( unsigned i=0; i < MAX_READ_ATTEMPTS; ++i )
        {
            {
                std::lock_guard<std::mutex> lock( wantedQueue->mutex() ); // LOCK

                if( wantedQueue->size() )
                {
                    std::unique_ptr<VideoPacket> packet( new VideoPacket() );
                    wantedQueue->front()->swap( *packet );
                    wantedQueue->pop_front();

                    if( encoderNumber )
                        packet->setLowQualityFlag();
                    return packet.release();
                }
            }

            std::chrono::milliseconds dura( SLEEP_DURATION_MS );
            std::this_thread::sleep_for( dura );
        }

        return new VideoPacket();
    }

    unsigned CameraManager::cameraId(const nxcip::CameraInfo& info)
    {
        std::string name( info.url );
        name.erase( name.find( DEVICE_PATTERN ), strlen( DEVICE_PATTERN ) );

        unsigned num;
        std::stringstream ss;
        ss << name;
        ss >> num;
        return num;
    }

    void CameraManager::driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const
    {
        if( m_device )
        {
            char verDriver[32] = "";
            char verAPI[32] = "";
            char verFWLink[32] = "";
            char verFWOFDM[32] = "";
            char xCompany[32] = "";
            char xModel[32] = "";

            m_device->info(verDriver, verAPI, verFWLink, verFWOFDM, xCompany, xModel);

            driverVersion = std::string(verDriver) + "-" + std::string(verAPI);
            fwVersion = std::string(verFWLink) + "-" + std::string(verFWOFDM);
            company = std::string(xCompany);
            model = std::string(xModel);
        }
    }

    void CameraManager::fillInfo(nxcip::CameraInfo& info) const
    {
        std::string driver, firmware, company, model;
        driverInfo( driver, firmware, company, model );

        driver += " ";
        driver += firmware;

        strncpy( info.modelName, model.c_str(), model.size() );
        strncpy( info.firmware, driver.c_str(), driver.size() );
    }
}
