#include <string>
#include <sstream>

#include "pacidal_it930x.h"
#include "camera_manager.h"
#include "media_encoder.h"

#include "video_packet.h"
#include "libav_wrap.h"

#include "discovery_manager.h"

namespace pacidal
{
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
}

namespace pacidal
{
    static const unsigned DEFAULT_FREQUENCY = 177000;
    static const unsigned MIN_SIGNAL_STRENGTH = 50; // 0..100
    static const unsigned MAX_READ_ATTEMPTS = 42;

    static const unsigned DEFAULT_FRAME_SIZE = PacidalIt930x::DEFAULT_PACKETS_NUM * PacidalIt930x::MPEG_TS_PACKET_SIZE;

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager( const nxcip::CameraInfo& info )
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
        if (devInited_)
            initEncoders();
    }

    CameraManager::~CameraManager()
    {
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
        m_device->lockChannel(DEFAULT_FREQUENCY);

        bool locked, presented;
        uint8_t quality, strength;
        m_device->statistic(locked, presented, quality, strength);

        // low signal == no data. Don't capture trash.
        if (strength < MIN_SIGNAL_STRENGTH)
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

        m_encQueues.resize( m_encoders.size() );
        encInited_ = true;
    }

    void CameraManager::resolution( unsigned encoderNum, nxcip::ResolutionInfo& outRes ) const
    {
        AVStream * stream = m_libAV->stream(encoderNum);

        if( stream )
        {
            outRes.maxFps = float(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;

            if( stream->codec )
            {
                outRes.resolution.width = stream->codec->width;     // coded_width ?
                outRes.resolution.height = stream->codec->height;   // coded_height ?
            }
        }
    }

    VideoPacket* CameraManager::nextPacket( unsigned encoderNumber )
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if( m_encQueues[encoderNumber].size() )
        {
            std::unique_ptr<VideoPacket> packet( new VideoPacket() );
            (*m_encQueues[encoderNumber].begin())->swap(*packet);
            m_encQueues[encoderNumber].pop_front();

            return packet.release();
        }

        // prevent infinite loop
        for( unsigned i=0; i < MAX_READ_ATTEMPTS; ++i )
        {
            unsigned streamNum;
            std::unique_ptr<VideoPacket> packet = nextDevPacket(streamNum);
            if (! packet)
                continue;

            if( streamNum )
                packet->setLowQualityFlag();

            if( streamNum == encoderNumber )
                return packet.release();

            std::shared_ptr<VideoPacket> sp( packet.release() );
            if( sp->isKey() )
                m_encQueues[streamNum].clear();
            m_encQueues[streamNum].push_back(sp);
        }

        return new VideoPacket();
    }

    std::unique_ptr<VideoPacket> CameraManager::nextDevPacket(unsigned& streamNum)
    {
        AVPacket avPacket;
        if( m_libAV->nextFrame(&avPacket) < 0 || avPacket.flags & AV_PKT_FLAG_CORRUPT )
        {
            m_libAV->freePacket(&avPacket);
            return std::unique_ptr<VideoPacket>();
        }

        std::unique_ptr<VideoPacket> packet( new VideoPacket(avPacket.data, avPacket.size) );
        packet->setTime(avPacket.pts);
        if( avPacket.flags & AV_PKT_FLAG_KEY )
            packet->setKeyFlag();

        streamNum = avPacket.stream_index;
        m_libAV->freePacket(&avPacket);
        return packet;
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
}
