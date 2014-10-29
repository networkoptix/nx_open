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
#include <sys/time.h>
#endif

#include <sys/epoll.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cassert>
#include <iostream>
#include <fstream>

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

#include <plugins/resource/third_party/motion_data_picture.h>
#include <utils/common/log.h>
#include <utils/common/synctime.h>

#include "ilp_video_packet.h"
#include "ilp_empty_packet.h"
#include "isd_audio_packet.h"
#include "linux_process_info.h"


//#define DUMP_VIDEO_STREAM
#ifdef DUMP_VIDEO_STREAM
#include <fcntl.h>

static const int ENCODER_NUM_TO_DUMP = 0;
int videoDumpFile = -1;
static const char* DUMP_FILE_NAME = "/media/mmcblk0p1/video.264";
#endif

static const int HIGH_QUALITY_ENCODER = 0;
static const int ENCODER_STREAM_TO_ADD_MOTION_TO = 1;   //1 - low stream
static const int64_t MOTION_TIMEOUT_USEC = 1000ll * 300;
static const int PTS_FREQUENCY = 90000;
static const unsigned int MAX_PTS_DRIFT = 90000 * 1.5;    //1500 ms
static const unsigned int DEFAULT_FRAME_DURATION = 3000;    //30 fps
static const int MSEC_IN_SEC = 1000;
static const int USEC_IN_SEC = 1000*1000;
static const int USEC_IN_MSEC = 1000;
static const int MAX_FRAMES_BETWEEN_TIME_RESYNC = 300;

struct TimeSynchronizationData
{
    int encoderThatInitializedThis;
    PtsToClockMapper::TimeSynchronizationData timeSyncData;
    QMutex mutex;

    TimeSynchronizationData()
    :
        encoderThatInitializedThis( -1 )
    {
    }
};

static TimeSynchronizationData timeSyncData;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderNum,
#ifndef NO_ISD_AUDIO
    const std::unique_ptr<AudioStreamReader>& audioStreamReader,
#endif
    const char* /*cameraUid*/ )
:
    m_refManager( parentRefManager ),
    m_encoderNum( encoderNum ),
    m_videoCodec(nxcip::CODEC_ID_NONE),
    m_lastVideoTime(0),
    m_lastMotionTime(0),
#ifndef NO_ISD_AUDIO
    m_audioStreamReader( audioStreamReader ),
    m_audioEventFD( eventfd( 0, EFD_SEMAPHORE ) ),
    m_audioEventFDRegistered( false ),
    m_audioEnabled(false),
    m_audioReceiverID(0),
    //m_audioCodec(nxcip::CODEC_ID_NONE),
#endif
    m_prevPts(0),
    m_timestampDelta(std::numeric_limits<int64_t>::max()),
    m_framesSinceTimeResync(MAX_FRAMES_BETWEEN_TIME_RESYNC),
    m_epollFD(-1),
    m_motionData(nullptr),
    m_ptsMapper(90000, &timeSyncData.timeSyncData, encoderNum),
    m_currentGopSizeBytes( 0 )
#ifdef DEBUG_OUTPUT
    ,m_totalFramesRead(0)
#endif
{
#ifdef DUMP_VIDEO_STREAM
    if( m_encoderNum == ENCODER_NUM_TO_DUMP )
        videoDumpFile = open( DUMP_FILE_NAME, O_CREAT | O_WRONLY | O_TRUNC | O_SYNC, S_IRUSR | S_IWUSR );
#endif

#ifdef DEBUG_OUTPUT
    m_frameTimer.invalidate();
#endif
}

StreamReader::~StreamReader()
{
    closeAllStreams();

    if( m_epollFD )
    {
        close( m_epollFD );
        m_epollFD = -1;
    }

#ifndef NO_ISD_AUDIO
    if( m_audioEventFD != -1 )
    {
        close( m_audioEventFD );
        m_audioEventFD = -1;
    }
#endif
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
    m_refCounter.fetchAndAddOrdered(1);
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    if( m_refCounter.fetchAndAddOrdered(-1)-1 == 0 )
        closeAllStreams();
    return m_refManager.releaseRef();
}

bool StreamReader::needMetaData()
{
    if (m_encoderNum == ENCODER_STREAM_TO_ADD_MOTION_TO) {
        if (m_lastVideoTime - m_lastMotionTime >= MOTION_TIMEOUT_USEC)
        {
            m_lastMotionTime = m_lastVideoTime;
            return true;
        }
    }
    return false;
}

void StreamReader::setMotionMask(const uint8_t* data)
{
    m_motionEstimation.setMotionMask(data);
}

void StreamReader::setAudioEnabled( bool audioEnabled )
{
#ifndef NO_ISD_AUDIO
    m_audioEnabled.store( audioEnabled, std::memory_order_relaxed );

    if( audioEnabled )
    {
        QMutexLocker lk( &m_mutex );
        if( m_audioReceiverID > 0 )
            m_audioStreamReader->removePacketReceiver( m_audioReceiverID );
        m_audioReceiverID = m_audioStreamReader->setPacketReceiver(
            std::bind( &StreamReader::onAudioDataAvailable, this, std::placeholders::_1 ) );
    }
#else
    Q_UNUSED( audioEnabled )
#endif
}

//int StreamReader::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
//{
//#ifndef NO_ISD_AUDIO
//    //QMutexLocker lk( &m_mutex );
//
//    //if( !m_audioFormat.get() )
//    //    return nxcip::NX_TRY_AGAIN;
//
//    //*audioFormat = *m_audioFormat.get();
//    *audioFormat = m_audioStreamReader->getAudioFormat();
//    return nxcip::NX_NO_ERROR;
//#else
//    Q_UNUSED( audioFormat )
//    return nxcip::NX_UNSUPPORTED_CODEC;
//#endif
//}

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    //std::cout << "ISD plugin getNextData started for encoder" << m_encoderNum << std::endl;

    const bool audioEnabledLocal = m_audioEnabled.load( std::memory_order_relaxed );

    const size_t cameraStreamsToRead = 
          1                                                                 //video
        //+ (m_encoderNum == ENCODER_STREAM_TO_ADD_MOTION_TO ? 1 : 0)         //motion
#ifndef NO_ISD_AUDIO
        //+ ((m_encoderNum == HIGH_QUALITY_ENCODER) && m_audioEnabled ? 1 : 0)  //audio
        + (audioEnabledLocal ? 1 : 0)  //audio
#endif
        ;

#ifndef NO_ISD_AUDIO
    if( audioEnabledLocal )
    {
        //assert( cameraStreamsToRead > 1 );
        //if( m_encoderNum == HIGH_QUALITY_ENCODER )
        //{
        //    if( !m_amux.get() )
        //    {
        //        //audio is not required
        //        int result = initializeAMux();
        //        if( result != nxcip::NX_NO_ERROR )
        //            return result;
        //        if( !registerFD( m_amux->GetFD() ) )
        //            return nxcip::NX_IO_ERROR;
        //    }
        //}
        //else if( m_audioStreamBridge && !m_audioStreamBridgeRegistered )
        //{
        //    if( !registerFD( m_audioStreamBridge->eventFD() ) )
        //        return nxcip::NX_IO_ERROR;
        //    m_audioStreamBridgeRegistered = true;
        //}
        if( !m_audioEventFDRegistered && (m_audioEventFD != -1) )
        {
            if( !registerFD( m_audioEventFD ) )
                return nxcip::NX_IO_ERROR;
            m_audioEventFDRegistered = true;
        }
    }
#endif

    if( !m_vmux.get() )
    {
        int result = initializeVMux();
        if( result != nxcip::NX_NO_ERROR )
            return result;
        if( cameraStreamsToRead > 1 )    //using epoll only if receiving 2 streams or more
            if( !registerFD( m_vmux->GetFD() ) )
                return nxcip::NX_IO_ERROR;
    }

    if( cameraStreamsToRead == 1 )
    {
        //not using epoll to maximize performance
        for( ;; )
        {
            const int result = getVideoPacket( lpPacket );
            if( result == nxcip::NX_TRY_AGAIN )
                continue;
            return result;
        }
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
                const int result = getVideoPacket( lpPacket );
                if( result == nxcip::NX_TRY_AGAIN )
                    continue;   //trying again
                return result;
            }
#ifndef NO_ISD_AUDIO
            //else if( m_amux.get() && eventsArray[i].data.fd == m_amux->GetFD() )
            else if( eventsArray[i].data.fd == m_audioEventFD )
            {
                const int result = getAudioPacket( lpPacket );
                if( result == nxcip::NX_TRY_AGAIN )
                    continue;   //trying again

                return result;
            }
#endif
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
    //ProcessStatus processStatus;
    //memset( &processStatus, 0, sizeof(processStatus) );
    //readProcessStatus( &processStatus );
    //std::cout<<"StreamReader::initializeVMux(). Initializing... VmPeak "<<processStatus.vmPeak<<", VmSize = "<<processStatus.vmSize<<std::endl;

    std::unique_ptr<Vmux> vmux( new Vmux() );

    //readProcessStatus( &processStatus );
    //std::cout<<"StreamReader::initializeVMux(). mark1. VmPeak "<<processStatus.vmPeak<<", VmSize = "<<processStatus.vmSize<<std::endl;

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
        std::cerr<<"ISD plugin: unsupported video format "<<stream_info.enc_type << " for encoder " << m_encoderNum << "\n";
        return nxcip::NX_INVALID_ENCODER_NUMBER;
    }

    rv = vmux->StartVideo (m_encoderNum);
    if (rv) {
        std::cerr << "ISD plugin: can't start video" << std::endl;
        return nxcip::NX_INVALID_ENCODER_NUMBER; // error
    }
#ifdef DEBUG_OUTPUT
    std::cout<<"ISD plugin: started video on encoder "<<m_encoderNum<<std::endl;
#endif
    m_currentTimestamp = 0;

    m_vmux = std::move(vmux);

    //std::cout<<"VMUX initialized. fd = "<<m_vmux->GetFD()<<"\n";
    //memset( &processStatus, 0, sizeof(processStatus) );
    //readProcessStatus( &processStatus );
    //std::cout<<"StreamReader::initializeVMux(). VMUX initialized. VmPeak "<<processStatus.vmPeak<<", VmSize = "<<processStatus.vmSize<<std::endl;

    return nxcip::NX_NO_ERROR;
}

int StreamReader::getVideoPacket( nxcip::MediaDataPacket** lpPacket )
{
#ifdef DEBUG_OUTPUT
    if( !m_frameTimer.isValid() )
        m_frameTimer.restart();
#endif

    vmux_frame_t frame;
    memset( &frame, 0, sizeof(frame) );
    int rv = m_vmux->GetFrame (&frame);
    if (rv) {
        const int errnoBak = errno;
        if( errnoBak == EINTR || errnoBak == EWOULDBLOCK || errnoBak == EAGAIN || errnoBak == EINPROGRESS )
            return nxcip::NX_TRY_AGAIN;
        std::cerr << "Can't read video frame. "<<rv<<", errno "<<errnoBak<<std::endl;
        if( m_epollFD != -1 )
            unregisterFD( m_vmux->GetFD() );
        m_vmux.reset();
        return nxcip::NX_IO_ERROR; // error
    }

#ifdef DEBUG_OUTPUT
    ++m_totalFramesRead;
    if( m_frameTimer.elapsed() > 5*1000 )
    {
        std::cout<<"encoder "<<m_encoderNum<<". fps "<<(m_totalFramesRead*1000 / m_frameTimer.elapsed())<<std::endl;
        m_frameTimer.restart();
        m_totalFramesRead = 0;
    }
#endif

    std::unique_ptr<ILPVideoPacket> videoPacket( new ILPVideoPacket(
        &m_allocator,
        0, // channel
        calcNextTimestamp(
            frame.vmux_info.PTS
#ifdef ABSOLUTE_FRAME_TIME_PRESENT
            , (qint64)frame.tv_sec * USEC_IN_SEC + frame.tv_usec
#else
            , QDateTime::currentMSecsSinceEpoch()
#endif
            ),
        (frame.vmux_info.pic_type == VMUX_IDR_FRAME ? nxcip::MediaDataPacket::fKeyPacket : 0) | 
            (m_encoderNum > 0 ? nxcip::MediaDataPacket::fLowQuality : 0),
        0, // cseq
        m_videoCodec ));
    videoPacket->resizeBuffer( frame.frame_size );
    if( !videoPacket->data() )
    {
        std::cerr<<"ISD plugin. Could not allocate "<<frame.frame_size<<" bytes for video frame"<<std::endl;
        m_vmux.reset();
        return nxcip::NX_IO_ERROR; // error
    }
    memcpy(videoPacket->data(), frame.frame_addr, frame.frame_size);

    //TODO #ak calculating GOP size in bytes
    if( frame.vmux_info.pic_type == VMUX_IDR_FRAME && m_currentGopSizeBytes > 0 )
        m_currentGopSizeBytes = 0;
    m_currentGopSizeBytes += frame.frame_size;

#ifdef DUMP_VIDEO_STREAM
    if( m_encoderNum == ENCODER_NUM_TO_DUMP ) 
    {
        std::cout<<"Dumping frame "<<frame.frame_size<<" bytes to "<<videoDumpFile<<std::endl;
        write( videoDumpFile, frame.frame_addr, frame.frame_size );
        fsync( videoDumpFile );
    }
#endif

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

void StreamReader::readMotion()
{
    if( !m_vmuxMotion.get() )
    {
        int result = initializeVMuxMotion();
        if( result != nxcip::NX_NO_ERROR )
            return;
    }

    vmux_frame_t frame;
    int rv = m_vmuxMotion->GetFrame (&frame);
    if (rv) {
        m_vmuxMotion.reset();
        return;
    }

    m_motionEstimation.analizeFrame(frame.frame_addr , motion_stream_info.width, motion_stream_info.height, motion_stream_info.pitch);
    MotionDataPicture* motionData = m_motionEstimation.getMotion();
    if( m_motionData )
        m_motionData->releaseRef();
    m_motionData = motionData;
}

MotionDataPicture* StreamReader::getMotionData()
{
    readMotion();
    MotionDataPicture* result = m_motionData;
    m_motionData = nullptr;
    return result;
}

int StreamReader::initializeVMuxMotion()
{
    std::unique_ptr<Vmux> newVmuxMotion( new Vmux() );

    int info_size = sizeof(motion_stream_info);
    int rv = newVmuxMotion->GetStreamInfo (Y_STREAM_SMALL, &motion_stream_info, &info_size);
    if (rv) {
        std::cerr << "can't get stream info for motion stream" << std::endl;
        return nxcip::NX_IO_ERROR; // error
    }

    //std::cout << "motion width=" << motion_stream_info.width << " height=" << motion_stream_info.height << " stride=" << motion_stream_info.pitch << std::endl;

    rv = newVmuxMotion->StartVideo (Y_STREAM_SMALL);
    if (rv) {
        return nxcip::NX_IO_ERROR; // error
    }

    m_vmuxMotion = std::move( newVmuxMotion );
    return nxcip::NX_NO_ERROR;
}

#ifndef NO_ISD_AUDIO
//int StreamReader::initializeAMux()
//{
//    std::unique_ptr<Amux> amux( new Amux() );
//    memset( &m_audioInfo, 0, sizeof(m_audioInfo) );
//
//    if( amux->GetInfo( &m_audioInfo ) )
//    {
//        std::cerr << "ISD plugin: can't get audio stream info\n";
//        return nxcip::NX_IO_ERROR;
//    }
//
//    switch( m_audioInfo.encoding )
//    {
//        case EncPCM:
//            m_audioCodec = nxcip::CODEC_ID_PCM_S16LE;
//            break;
//        case EncUlaw:
//            m_audioCodec = nxcip::CODEC_ID_PCM_MULAW;
//            break;
//        case EncAAC:
//            m_audioCodec = nxcip::CODEC_ID_AAC;
//            break;
//        default:
//            std::cerr << "ISD plugin: unsupported audio codec: "<<m_audioInfo.encoding<<"\n";
//            return nxcip::NX_UNSUPPORTED_CODEC;
//    }
//
//    if( amux->StartAudio() )
//    {
//        std::cerr << "ISD plugin: can't start audio stream\n";
//        return nxcip::NX_IO_ERROR;
//    }
//
//    //std::cout<<"AMUX initialized. Codec type "<<m_audioCodec<<" fd = "<<amux->GetFD()<<"\n";
//
//    m_amux = std::move(amux);
//    return nxcip::NX_NO_ERROR;
//}
//
//int StreamReader::getAudioPacket( nxcip::MediaDataPacket** lpPacket )
//{
//    int audioBytesAvailable = m_amux->BytesAvail();
//    if( audioBytesAvailable <= 0 )
//    {
//        std::cerr<<"ISD plugin: no audio bytes available\n";
//        m_amux.reset();
//        return nxcip::NX_IO_ERROR;
//    }
//
//    std::unique_ptr<ISDAudioPacket> audioPacket( new ISDAudioPacket(
//        1,  //channel number
//        m_lastVideoTime,
//        m_audioCodec ) );
//    audioPacket->reserve( audioBytesAvailable );
//
//    int bytesRead = m_amux->ReadAudio( (char*)audioPacket->data(), audioPacket->capacity() );
//    if( bytesRead <= 0 )
//    {
//        std::cerr<<"ISD plugin: failed to read audio packet\n";
//        m_amux.reset();
//        return nxcip::NX_IO_ERROR;
//    }
//
//    audioPacket->setDataSize( bytesRead );
//
//    if( !m_audioFormat.get() )
//        fillAudioFormat( *audioPacket.get() );
//
//    //std::cout<<"Got audio packet. size "<<audioPacket->dataSize()<<", pts "<<audioPacket->timestamp()<<"\n";
//
//    *lpPacket = audioPacket.release();
//    return nxcip::NX_NO_ERROR;
//}
//
//void StreamReader::fillAudioFormat( const ISDAudioPacket& /*audioPacket*/ )
//{
//    QMutexLocker lk( &m_mutex );
//
//    m_audioFormat.reset( new nxcip::AudioFormat() );
//
//    m_audioFormat->compressionType = m_audioCodec;
//    m_audioFormat->sampleRate = m_audioInfo.sample_rate;
//    m_audioFormat->bitrate = m_audioInfo.bit_rate;
//
//    m_audioFormat->channels = 1;
//    switch( m_audioCodec )
//    {
//        case nxcip::CODEC_ID_AAC:
//        {
//            //TODO/IMPL parsing ADTS header to get sample rate
//            break;
//        }
//
//        //case nxcip::CODEC_ID_PCM_S16LE:
//        //case nxcip::CODEC_ID_PCM_MULAW:
//        default:
//            break;
//    }
//
//    //std::cout<<"Audio format: sample_rate "<<m_audioInfo.sample_rate<<", bitrate "<<m_audioInfo.bit_rate<<"\n";
//}

void StreamReader::onAudioDataAvailable( const std::shared_ptr<AudioData>& audioDataPtr )
{
    QMutexLocker lk( &m_mutex );
    uint64_t increment = 1;
    if( write( m_audioEventFD, &increment, sizeof(increment) ) == -1 )
        return; //in case of error does not saving audio data
    //TODO #ak limit m_audioPackets queue size
    m_audioPackets.push_back( audioDataPtr );
}

int StreamReader::getAudioPacket( nxcip::MediaDataPacket** lpPacket )
{
    uint64_t decrement = 0;
    if( read( m_audioEventFD, &decrement, sizeof(decrement) ) == -1 )
        return nxcip::NX_IO_ERROR;

    std::shared_ptr<AudioData> audioDataPtr;
    {
        QMutexLocker lk( &m_mutex );
        if( m_audioPackets.empty() )
            return nxcip::NX_TRY_AGAIN;
        audioDataPtr = std::move(m_audioPackets.front());
        m_audioPackets.pop_front();
    }

    std::unique_ptr<ISDAudioPacket> audioPacket( new ISDAudioPacket(
        1,  //channel number
        m_lastVideoTime ) );
    audioPacket->setData( std::move(audioDataPtr) );

    *lpPacket = audioPacket.release();
    return nxcip::NX_NO_ERROR;
}

#endif

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

void StreamReader::closeAllStreams()
{
#ifdef DUMP_VIDEO_STREAM
    if( m_encoderNum == ENCODER_NUM_TO_DUMP )
    {
        close( videoDumpFile );
        videoDumpFile = -1;
    }
#endif

    if( m_vmux.get() )
    {
        unregisterFD( m_vmux->GetFD() );
        m_vmux.reset();
#ifdef DEBUG_OUTPUT
        std::cout<<"ISD plugin: stopped video on encoder "<<m_encoderNum<<std::endl;
#endif
    }

    m_vmuxMotion.reset();

#ifndef NO_ISD_AUDIO
    if( m_audioReceiverID > 0 )
    {
        m_audioStreamReader->removePacketReceiver( m_audioReceiverID );
        m_audioReceiverID = 0;
    }
    //if( m_amux.get() )
    //{
    //    unregisterFD( m_amux->GetFD() );
    //    m_amux.reset();
    //}
#endif
}

int64_t StreamReader::calcNextTimestamp( int32_t pts, int64_t absoluteSourceTimeUSec )
{
    if( m_framesSinceTimeResync >= MAX_FRAMES_BETWEEN_TIME_RESYNC )
    {
        QMutexLocker lk( &timeSyncData.mutex );

        if( timeSyncData.encoderThatInitializedThis == -1 || timeSyncData.encoderThatInitializedThis == m_encoderNum )
        {
            resyncTime( absoluteSourceTimeUSec );
            timeSyncData.encoderThatInitializedThis = m_encoderNum;
        }

        m_ptsMapper.updateTimeMapping( pts, absoluteSourceTimeUSec );
        m_framesSinceTimeResync = 0;
    }
    ++m_framesSinceTimeResync;

    return m_ptsMapper.getTimestamp( pts );
}

void StreamReader::resyncTime( int64_t absoluteSourceTimeUSec )
{
    struct timeval currentTime;
    memset( &currentTime, 0, sizeof(currentTime) );
    gettimeofday( &currentTime, NULL );
    timeSyncData.timeSyncData.mapLocalTimeToSourceAbsoluteTime( 
        qnSyncTime->currentMSecsSinceEpoch()*USEC_IN_MSEC - ((int64_t)currentTime.tv_sec*USEC_IN_SEC + currentTime.tv_usec - absoluteSourceTimeUSec),
        absoluteSourceTimeUSec );

    NX_LOG( lit("ISD plugin. Primary stream time sync. Current local time %1:%2, frame time %3:%4").
        arg(currentTime.tv_sec).arg(currentTime.tv_usec).arg((absoluteSourceTimeUSec/USEC_IN_SEC)).
        arg(absoluteSourceTimeUSec % USEC_IN_SEC), cl_logDEBUG2 );
}
