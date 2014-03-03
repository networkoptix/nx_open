/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#else
#include <time.h>
#include <unistd.h>
#endif

#include <sys/epoll.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cassert>
#include <iostream>
#include <fstream>

#include <QDateTime>

#include <plugins/resources/third_party/motion_data_picture.h>

#include "ilp_video_packet.h"
#include "ilp_empty_packet.h"
#include "isd_audio_packet.h"


//if defined, video frames receive curernt system time as pts
//#define USE_SYSTEM_CLOCK


static const int HIGH_QUALITY_ENCODER = 0;
static const int64_t MOTION_TIMEOUT_USEC = 1000ll * 300;

StreamReader::StreamReader(nxpt::CommonRefManager* const parentRefManager, int encoderNum)
:
    m_refManager( parentRefManager ),
    m_encoderNum( encoderNum ),
    m_videoCodec(nxcip::CODEC_ID_NONE),
    m_lastVideoTime(0),
    m_lastMotionTime(0),
    m_vmux(0),
    m_vmux_motion(0),
    m_amux(0),
    m_audioCodec(nxcip::CODEC_ID_NONE),
    m_prevPts(0),
    m_ptsDelta(0),
    m_audioEnabled(false),
    m_epollFD(-1)
{
}

StreamReader::~StreamReader()
{
    if( m_vmux )
    {
        unregisterFD( m_vmux->GetFD() );
        delete m_vmux;
        m_vmux = 0;
    }

    delete m_vmux_motion;
    m_vmux_motion = 0;

    if( m_amux )
    {
        unregisterFD( m_amux->GetFD() );
        delete m_amux;
        m_amux = 0;
    }

    if( m_epollFD )
    {
        close( m_epollFD );
        m_epollFD = 0;
    }
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

bool StreamReader::needMetaData()
{
    if (m_encoderNum == 1) {
        if (m_lastVideoTime - m_lastMotionTime >= MOTION_TIMEOUT_USEC)
        {
            m_lastMotionTime = m_lastVideoTime;
            return true;
        }
    }
    return false;
}

MotionDataPicture* StreamReader::getMotionData()
{
    if (!m_vmux_motion)
    {
        m_vmux_motion = new Vmux();
        int info_size = sizeof(motion_stream_info);
        int rv = m_vmux_motion->GetStreamInfo (Y_STREAM_SMALL, &motion_stream_info, &info_size);
        if (rv) {
            std::cerr << "can't get stream info for motion stream" << std::endl;
            delete m_vmux_motion;
            m_vmux_motion = 0;
            return 0; // error
        }

        std::cout << "motion width=" << motion_stream_info.width << " height=" << motion_stream_info.height << " stride=" << motion_stream_info.pitch << std::endl;

        rv = m_vmux_motion->StartVideo (Y_STREAM_SMALL);
        if (rv) {
            delete m_vmux_motion;
            m_vmux_motion = 0;
            return 0; // error
        }
    }

    vmux_frame_t frame;
    int rv = m_vmux_motion->GetFrame (&frame);
    if (rv) {
        delete m_vmux_motion;
        m_vmux_motion = 0; // close motion stream
        return 0; // return error
    }

    m_motionEstimation.analizeFrame(frame.frame_addr , motion_stream_info.width, motion_stream_info.height, motion_stream_info.pitch);
    return m_motionEstimation.getMotion();
}

void StreamReader::setMotionMask(const uint8_t* data)
{
    m_motionEstimation.setMotionMask(data);
}

void StreamReader::setAudioEnabled( bool audioEnabled )
{
    m_audioEnabled = audioEnabled;
}

int StreamReader::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    if( !m_audioFormat.get() )
        return nxcip::NX_TRY_AGAIN;

    *audioFormat = *m_audioFormat.get();
    return nxcip::NX_NO_ERROR;
}

struct SharedStreamData
{
    std::mutex mutex;
    int64_t ptsBase;
    int64_t baseClock;

    SharedStreamData()
    :
        ptsBase( -1 ),
        baseClock( -1 )
    {
    }
};

static SharedStreamData m_sharedStreamData;

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    //std::cout << "ISD plugin getNextData started for encoder" << m_encoderNum << std::endl;

    if( !m_amux && (m_encoderNum == HIGH_QUALITY_ENCODER) && m_audioEnabled )
    {
        //audio is not required
        int result = initializeAMux();
        if( result == nxcip::NX_NO_ERROR )
            if( !registerFD( m_amux->GetFD() ) )
                return nxcip::NX_IO_ERROR;
    }

    if( !m_vmux )
    {
        int result = initializeVMux();
        if( result != nxcip::NX_NO_ERROR )
            return result;
        if( m_amux )    //using epoll only if receiving 2 streams
            if( !registerFD( m_vmux->GetFD() ) )
                return nxcip::NX_IO_ERROR;
    }

    if( !m_amux )
    {
        //not using epoll to maximize performance
        return getVideoPacket( lpPacket );
    }

    assert( m_epollFD != -1 );
    //polling audio and video streams
    for( ;; )
    {
        static const int EVENTS_ARRAY_CAPACITY = 2;
        epoll_event eventsArray[EVENTS_ARRAY_CAPACITY];

        int result = epoll_wait( m_epollFD, eventsArray, EVENTS_ARRAY_CAPACITY, -1 );
        if( result < 0 )
        {
            if( errno == EINTR )
                continue;
            //TODO/IMPL reinitialize?
            return nxcip::NX_IO_ERROR;
        }
        else if( result == 0 )
        {
            //timeout
            continue;
        }

        //getting corresponding packet
        for( int i = 0; i < result; ++i )
        {
            //TODO/IMPL check eventsArray[i].events

            if( eventsArray[i].data.fd == m_vmux->GetFD() )
            {
                return getVideoPacket( lpPacket );
            }
            else if( eventsArray[i].data.fd == m_amux->GetFD() )
            {
                return getAudioPacket( lpPacket );
            }
            else
            {
                assert( false );
                return nxcip::NX_IO_ERROR;
            }
        }
    }
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}

int StreamReader::initializeVMux()
{
    std::unique_ptr<Vmux> vmux( new Vmux() );

    vmux_stream_info_t stream_info;
    int rv = 0;

    int info_size = sizeof(stream_info);
    rv = vmux->GetStreamInfo (m_encoderNum, &stream_info, &info_size);
    if (rv) {
        std::cerr << "ISD plugin: can't get video stream info" << std::endl;
        return nxcip::NX_INVALID_ENCODER_NUMBER; // error
    }
    if (stream_info.enc_type == VMUX_ENC_TYPE_H264)
        m_videoCodec = nxcip::CODEC_ID_H264;
    else if (stream_info.enc_type == VMUX_ENC_TYPE_MJPG)
        m_videoCodec = nxcip::CODEC_ID_MJPEG;
    else
    {
        std::cerr<<"ISD plugin: unsupported video format "<<stream_info.enc_type<<"\n";
        return nxcip::NX_INVALID_ENCODER_NUMBER;
    }

    rv = vmux->StartVideo (m_encoderNum);
    if (rv) {
        std::cerr << "ISD plugin: can't start video" << std::endl;
        return nxcip::NX_INVALID_ENCODER_NUMBER; // error
    }
    m_firstFrameTime = 0;

    m_vmux = vmux.release();

    //std::cout<<"VMUX initialized. fd = "<<m_vmux->GetFD()<<"\n";

    return nxcip::NX_NO_ERROR;
}

int StreamReader::initializeAMux()
{
    std::auto_ptr<Amux> amux( new Amux() );
    memset( &m_audioInfo, 0, sizeof(m_audioInfo) );

    if( amux->GetInfo( &m_audioInfo ) )
    {
        std::cerr << "ISD plugin: can't get audio stream info\n";
        return nxcip::NX_IO_ERROR;
    }

    switch( m_audioInfo.encoding )
    {
        case EncPCM:
            m_audioCodec = nxcip::CODEC_ID_PCM_S16LE;
            break;
        case EncUlaw:
            m_audioCodec = nxcip::CODEC_ID_PCM_MULAW;
            break;
        case EncAAC:
            m_audioCodec = nxcip::CODEC_ID_AAC;
            break;
        default:
            std::cerr << "ISD plugin: unsupported audio codec: "<<m_audioInfo.encoding<<"\n";
            return nxcip::NX_UNSUPPORTED_CODEC;
    }

    if( amux->StartAudio() )
    {
        std::cerr << "ISD plugin: can't start audio stream\n";
        return nxcip::NX_IO_ERROR;
    }

    //std::cout<<"AMUX initialized. Codec type "<<m_audioCodec<<" fd = "<<amux->GetFD()<<"\n";

    m_amux = amux.release();
    return nxcip::NX_NO_ERROR;
}

int StreamReader::getVideoPacket( nxcip::MediaDataPacket** lpPacket )
{
    vmux_frame_t frame;
    int rv = 0;

    rv = m_vmux->GetFrame (&frame);
    if (rv) {
        std::cerr << "Can't read video frame" << std::endl;
        if( m_epollFD != -1 )
            unregisterFD( m_vmux->GetFD() );
        delete m_vmux;
        m_vmux = 0;
        return nxcip::NX_IO_ERROR; // error
    }

    if (frame.vmux_info.pic_type == 1) {
        //std::cout << "I-frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << std::endl;
    }
    //std::cout << "frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << "encoder=" << m_encoderNum << std::endl;
    //    std::cout<<"encoder "<<m_encoderNum<<" frame pts "<<frame.vmux_info.PTS<<"\n";

    std::auto_ptr<ILPVideoPacket> videoPacket( new ILPVideoPacket(
        0, // channel
        calcNextTimestamp(frame.vmux_info.PTS),
        (frame.vmux_info.pic_type == 1 ? nxcip::MediaDataPacket::fKeyPacket : 0) | 
            (m_encoderNum > 0 ? nxcip::MediaDataPacket::fLowQuality : 0),
        0, // cseq
        m_videoCodec )); 
    videoPacket->resizeBuffer( frame.frame_size );
    memcpy(videoPacket->data(), frame.frame_addr, frame.frame_size);
    m_lastVideoTime = videoPacket->timestamp();
    if (needMetaData()) {
        MotionDataPicture* motionData = getMotionData();
        if (motionData) {
            videoPacket->setMotionData( motionData );
            motionData->releaseRef();   //videoPacket takes reference to motionData
        }
    }

    *lpPacket = videoPacket.release();
    return nxcip::NX_NO_ERROR;
}

int StreamReader::getAudioPacket( nxcip::MediaDataPacket** lpPacket )
{
    int audioBytesAvailable = m_amux->BytesAvail();
    if( audioBytesAvailable <= 0 )
    {
        std::cerr<<"ISD plugin: no audio bytes available\n";
        return nxcip::NX_IO_ERROR;
    }

    std::auto_ptr<ISDAudioPacket> audioPacket( new ISDAudioPacket(
        0,  //channel number
        m_lastVideoTime,
        m_audioCodec ) );
    audioPacket->reserve( audioBytesAvailable );

    int bytesRead = m_amux->ReadAudio( (char*)audioPacket->data(), audioPacket->capacity() );
    if( bytesRead <= 0 )
    {
        std::cerr<<"ISD plugin: failed to read audio packet\n";
        return nxcip::NX_IO_ERROR;
    }

    audioPacket->setDataSize( bytesRead );

    if( !m_audioFormat.get() )
        fillAudioFormat( *audioPacket.get() );

    //std::cout<<"Got audio packet. size "<<audioPacket->dataSize()<<", pts "<<audioPacket->timestamp()<<"\n";

    *lpPacket = audioPacket.release();
    return nxcip::NX_NO_ERROR;
}

bool StreamReader::registerFD( int fd )
{
    if( m_epollFD == -1 )
        m_epollFD = epoll_create( 32 );

    epoll_event _event;
    memset( &_event, 0, sizeof(_event) );
    _event.data.fd = fd;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
    return epoll_ctl( m_epollFD, EPOLL_CTL_ADD, fd, &_event ) == 0;
}

void StreamReader::unregisterFD( int fd )
{
    if( m_epollFD != -1 )
        epoll_ctl( m_epollFD, EPOLL_CTL_DEL, fd, NULL );
}

void StreamReader::fillAudioFormat( const ISDAudioPacket& audioPacket )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    m_audioFormat.reset( new nxcip::AudioFormat() );

    m_audioFormat->compressionType = m_audioCodec;
    m_audioFormat->sampleRate = m_audioInfo.sample_rate;
    m_audioFormat->bitrate = m_audioInfo.bit_rate;

    m_audioFormat->channels = 1;
    switch( m_audioCodec )
    {
        case nxcip::CODEC_ID_AAC:
        {
            //TODO/IMPL parsing ADTS header to get sample rate
            break;
        }

        //case nxcip::CODEC_ID_PCM_S16LE:
        //case nxcip::CODEC_ID_PCM_MULAW:
        default:
            break;
    }

    //std::cout<<"Audio format: sample_rate "<<m_audioInfo.sample_rate<<", bitrate "<<m_audioInfo.bit_rate<<"\n";
}

static const unsigned int MAX_PTS_DRIFT = 63000;    //700 ms
static const unsigned int DEFAULT_FRAME_DURATION = 3000;    //30 fps

int64_t StreamReader::calcNextTimestamp( const unsigned int pts )
{
    if( m_firstFrameTime == 0 )
    {
        std::unique_lock<std::mutex> lk( m_sharedStreamData.mutex );
        if( m_sharedStreamData.ptsBase == -1 )
        {
            m_sharedStreamData.ptsBase = pts;
            m_sharedStreamData.baseClock = QDateTime::currentMSecsSinceEpoch() * 1000ll;
            //std::cout<<"1: m_sharedStreamData.ptsBase = "<<m_sharedStreamData.ptsBase<<", "<<m_sharedStreamData.baseClock<<"\n";
        }
        else
        {
            m_ptsDelta = ((pts - m_sharedStreamData.ptsBase) * 1000000ll) / 90000 +
                ((QDateTime::currentMSecsSinceEpoch() * 1000ll) - m_sharedStreamData.baseClock);
            //std::cout<<"2: PTS = "<<pts<<", delta "<<m_ptsDelta<<"\n";
        }

        m_firstFrameTime = QDateTime::currentMSecsSinceEpoch() * 1000ll;
        //m_firstFrameTime -= (int64_t(pts) * 1000000ll) / 90000;
        m_prevPts = pts;
    }

    if( !((pts - m_prevPts < MAX_PTS_DRIFT) || (m_prevPts - pts < MAX_PTS_DRIFT)) )
    {
        //pts discontinuity
        pts = m_prevPts + DEFAULT_FRAME_DURATION;
    }

    m_firstFrameTime += (int64_t(pts - m_prevPts) * 1000000ll) / 90000; //TODO dword wrap
    m_prevPts = pts;
    return
#ifdef USE_SYSTEM_CLOCK
        QDateTime::currentMSecsSinceEpoch() * 1000ll;
#else
        //(int64_t(pts) * 1000000ll) / 90000 + m_firstFrameTime,
        m_firstFrameTime + m_ptsDelta;
#endif
}
