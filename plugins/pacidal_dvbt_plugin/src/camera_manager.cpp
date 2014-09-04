#include <string>

#include "../src/utils/common/log.h"

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
    static const unsigned MIN_SIGNAL_STRENGTH = 20; // 0..100
    static const unsigned MAX_SAVED_PACKETS = 60;   // 30 fps => 2 sec
    static const unsigned MAX_READ_ATTEMPTS = 30;

    static const unsigned DEFAULT_FRAME_SIZE = PacidalIt930x::DEFAULT_PACKETS_NUM * PacidalIt930x::MPEG_TS_PACKET_SIZE;

    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager( const nxcip::CameraInfo& info )
    :
        m_refManager( DiscoveryManager::refManager() ),
        m_info( info ),
        m_device(new PacidalIt930x(info.uid[0]-'0')),
        devInited_(false),
        encInited_(false)
    {
        std::string msg = "PACIDAL - CameraManager(), id: ";
        msg += '0' + info.auxiliaryData[0];

        NX_LOG( msg.c_str(), cl_logINFO );

        initDevice();
        if (devInited_)
            initEncoders();
    }

    CameraManager::~CameraManager()
    {
        NX_LOG( "PACIDAL - ~CameraManager()", cl_logINFO );
    }

    void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        NX_LOG( "PACIDAL - queryInterface()", cl_logINFO );

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
        NX_LOG( "PACIDAL - getEncoderCount()", cl_logINFO );

        *encoderCount = m_encoders.size();
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getEncoder
    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        NX_LOG( "PACIDAL - getEncoder()", cl_logINFO );

        if( !encInited_ || encoderIndex > m_encoders.size() )
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        m_encoders[encoderIndex]->addRef();
        *encoderPtr = m_encoders[encoderIndex].get();
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraInfo
    int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        NX_LOG( "PACIDAL - getCameraInfo()", cl_logINFO );

        memcpy( info, &m_info, sizeof(m_info) );
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
    int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        NX_LOG( "PACIDAL - getCameraCapabilities()", cl_logINFO );

        *capabilitiesMask =
                nxcip::BaseCameraManager::nativeMediaStreamCapability |
                nxcip::BaseCameraManager::primaryStreamSoftMotionCapability;
        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::setCredentials
    void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
    {
        NX_LOG( "PACIDAL - setCredentials()", cl_logINFO );
    }

    //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
    int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
    {
        NX_LOG( "PACIDAL - setAudioEnabled()", cl_logINFO );

        return nxcip::NX_NO_ERROR;
    }

    //!Implementation of nxcip::BaseCameraManager::getPTZManager
    nxcip::CameraPtzManager* CameraManager::getPtzManager() const
    {
        NX_LOG( "PACIDAL - getPtzManager()", cl_logINFO );

        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
    {
        NX_LOG( "PACIDAL - getCameraMotionDataProvider()", cl_logINFO );

        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
    {
        NX_LOG( "PACIDAL - getCameraRelayIOManager()", cl_logINFO );

        return nullptr;
    }

    //!Implementation of nxcip::BaseCameraManager::getLastErrorString
    void CameraManager::getLastErrorString( char* errorString ) const
    {
        NX_LOG( "PACIDAL - getLastErrorString()", cl_logINFO );

        if (errorString)
            errorString[0] = '\0';
    }

    int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
    {
        NX_LOG( "PACIDAL - createDtsArchiveReader()", cl_logINFO );

        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
    {
        NX_LOG( "PACIDAL - find()", cl_logINFO );

        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
    {
        NX_LOG( "PACIDAL - setMotionMask()", cl_logINFO );

        return nxcip::NX_NOT_IMPLEMENTED;
    }

    // internals

    void CameraManager::initDevice()
    {
        NX_LOG( "PACIDAL - initDevice()", cl_logINFO );

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
        NX_LOG( "PACIDAL - initEncoders()", cl_logINFO );

        m_libAV.reset(new LibAV(m_devReader.get(), readDevice));

        m_encoders.reserve( m_libAV->streamsCount() );
        for( unsigned i=0; i<m_libAV->streamsCount(); ++i )
            m_encoders.push_back( std::make_shared<MediaEncoder>(this, i) );

        m_encQueues.resize( m_encoders.size() );
        encInited_ = true;
    }

    void CameraManager::resolution( unsigned encoderNum, nxcip::ResolutionInfo& outRes ) const
    {
        NX_LOG( "PACIDAL - resolution()", cl_logINFO );

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
            if (m_encQueues[streamNum].size() >= MAX_SAVED_PACKETS)
            {
                m_encQueues[streamNum].pop_front();
                while (m_encQueues[streamNum].size() && ! (*m_encQueues[streamNum].begin())->isKey())
                    m_encQueues[streamNum].pop_front();
            }
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
}
