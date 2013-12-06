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

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fstream>
#include <memory>
#include <mutex>

#include "ilp_video_packet.h"
#include "ilp_empty_packet.h"
#include <plugins/resources/third_party/motion_data_picture.h>
#include <iostream>

#include <QDateTime>

#define GENERATE_RANDOM_MOTION
#ifdef GENERATE_RANDOM_MOTION
static const unsigned int MOTION_PRESENCE_CHANCE_PERCENT = 70;
#endif

static const int64_t MOTION_TIMEOUT_USEC = 1000ll * 300;

StreamReader::StreamReader(nxpt::CommonRefManager* const parentRefManager, int encoderNum)
:
    m_refManager( parentRefManager ),
    m_encoderNum( encoderNum ),
    m_codec(nxcip::CODEC_ID_NONE),
    m_lastVideoTime(0),
    m_lastMotionTime(0),
    vmux_motion(0),
    vmux(0),
    m_prevPts(0),
    m_ptsDelta(0)
{
}

StreamReader::~StreamReader()
{
    delete vmux_motion;
    delete vmux;
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
    if (!vmux_motion)
    {
        vmux_motion = new Vmux();
        int info_size = sizeof(motion_stream_info);
        int rv = vmux_motion->GetStreamInfo (Y_STREAM_SMALL, &motion_stream_info, &info_size);
        if (rv) {
            std::cout << "can't get stream info for motion stream" << std::endl;
            delete vmux_motion;
            vmux_motion = 0;
            return 0; // error
        }

        std::cout << "motion width=" << motion_stream_info.width << " height=" << motion_stream_info.height << " stride=" << motion_stream_info.pitch << std::endl;

        rv = vmux_motion->StartVideo (Y_STREAM_SMALL);
        if (rv) {
            delete vmux_motion;
            vmux_motion = 0;
            return 0; // error
        }
    }

    vmux_frame_t frame;
    int rv = vmux_motion->GetFrame (&frame);
    if (rv) {
        delete vmux_motion;
        vmux_motion = 0; // close motion stream
        return 0; // return error
    }

    m_motionEstimation.analizeFrame(frame.frame_addr , motion_stream_info.width, motion_stream_info.height, motion_stream_info.pitch);
    return m_motionEstimation.getMotion();
}

void StreamReader::setMotionMask(const uint8_t* data)
{
    m_motionEstimation.setMotionMask(data);
}

#define USE_SYSTEM_CLOCK

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

    vmux_frame_t frame;
    vmux_stream_info_t stream_info;
    int rv = 0;

    if (!vmux)
    {
        vmux = new Vmux();
        int info_size = sizeof(stream_info);
        rv = vmux->GetStreamInfo (m_encoderNum, &stream_info, &info_size);
        if (rv) {
            std::cout << "ISD plugin: can't get stream info" << std::endl;
            delete vmux;
            vmux = 0;
            return nxcip::NX_INVALID_ENCODER_NUMBER; // error
        }
        if (stream_info.enc_type == VMUX_ENC_TYPE_H264)
            m_codec = nxcip::CODEC_ID_H264;
        else if (stream_info.enc_type == VMUX_ENC_TYPE_MJPG)
            m_codec = nxcip::CODEC_ID_MJPEG;
        else
        {
            std::cerr<<"ISD plugin: unsupported video format "<<stream_info.enc_type<<"\n";
            delete vmux;
            vmux = 0;
            return nxcip::NX_INVALID_ENCODER_NUMBER;
        }

        rv = vmux->StartVideo (m_encoderNum);
        if (rv) {
            std::cout << "ISD plugin: can't start video" << std::endl;
            delete vmux;
            vmux = 0;
            return nxcip::NX_INVALID_ENCODER_NUMBER; // error
        }
        m_firstFrameTime = 0;
    }

    rv = vmux->GetFrame (&frame);
    if (rv) {
        std::cout << "Can't read video frame" << std::endl;
        delete vmux;
        vmux = 0;
        return nxcip::NX_IO_ERROR; // error
    }

    if (frame.vmux_info.pic_type == 1) {
        //std::cout << "I-frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << std::endl;
    }
    //std::cout << "frame pts = " << frame.vmux_info.PTS << "pic_type=" << frame.vmux_info.pic_type << "encoder=" << m_encoderNum << std::endl;
    //    std::cout<<"encoder "<<m_encoderNum<<" frame pts "<<frame.vmux_info.PTS<<"\n";

    if( m_firstFrameTime == 0 )
    {
        std::unique_lock<std::mutex> lk( m_sharedStreamData.mutex );
        if( m_sharedStreamData.ptsBase == -1 )
        {
            m_sharedStreamData.ptsBase = frame.vmux_info.PTS;
            m_sharedStreamData.baseClock = QDateTime::currentMSecsSinceEpoch() * 1000ll;
            //std::cout<<"1: m_sharedStreamData.ptsBase = "<<m_sharedStreamData.ptsBase<<", "<<m_sharedStreamData.baseClock<<"\n";
        }
        else
        {
            m_ptsDelta = ((frame.vmux_info.PTS - m_sharedStreamData.ptsBase) * 1000000ll) / 90000 +
                ((QDateTime::currentMSecsSinceEpoch() * 1000ll) - m_sharedStreamData.baseClock);
            //std::cout<<"2: PTS = "<<frame.vmux_info.PTS<<", delta "<<m_ptsDelta<<"\n";
        }
    }

    if (m_firstFrameTime == 0) {
        m_firstFrameTime = QDateTime::currentMSecsSinceEpoch() * 1000ll;
        //m_firstFrameTime -= (int64_t(frame.vmux_info.PTS) * 1000000ll) / 90000;
        m_prevPts = frame.vmux_info.PTS;
    }
    m_firstFrameTime += (int64_t(frame.vmux_info.PTS - m_prevPts) * 1000000ll) / 90000; //TODO dword wrap
    std::auto_ptr<ILPVideoPacket> videoPacket( new ILPVideoPacket(
        0, // channel
        //(int64_t(frame.vmux_info.PTS) * 1000000ll) / 90000 + m_firstFrameTime,
#ifdef USE_SYSTEM_CLOCK
        QDateTime::currentMSecsSinceEpoch() * 1000ll,
#else
        m_firstFrameTime + m_ptsDelta,
#endif
        //currentTime,
        (frame.vmux_info.pic_type == 1 ? nxcip::MediaDataPacket::fKeyPacket : 0) | 
            (m_encoderNum > 0 ? nxcip::MediaDataPacket::fLowQuality : 0),
        0, // cseq
        m_codec)); 
    m_prevPts = frame.vmux_info.PTS;
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

void StreamReader::interrupt()
{
    //TODO/IMPL
}
