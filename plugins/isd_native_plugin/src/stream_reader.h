/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <stdint.h>

#include <memory>
#include <mutex>

#ifndef NO_ISD_AUDIO
#include <isd/amux/amux_iface.h>
#endif
#include <isd/vmux/vmux_iface.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <utils/network/rtpsession.h>

#include "isd_motion_estimation.h"

//#define DUPLICATE_MOTION_TO_HIGH_STREAM


class ISDAudioPacket;
class MotionDataPicture;

//!Reads picture files from specified directory as video-stream
class StreamReader
:
    public nxcip::StreamReader
{
public:
    /*!
        \param liveMode In this mode, plays all pictures in a loop
    */
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderNum,
        const char* cameraUid );
    virtual ~StreamReader();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation nxcip::StreamReader::getNextData
    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    //!Implementation nxcip::StreamReader::interrupt
    virtual void interrupt() override;

    void setMotionMask( const uint8_t* data );
    void setAudioEnabled( bool audioEnabled );
    int getAudioFormat( nxcip::AudioFormat* audioFormat ) const;

private:
    bool needMetaData();
    MotionDataPicture* getMotionData();

private:
    nxpt::CommonRefManager m_refManager;
    int m_encoderNum;
    nxcip::CompressionType m_videoCodec;
    nxcip::UsecUTCTimestamp m_lastVideoTime;
    nxcip::UsecUTCTimestamp m_lastMotionTime;
    Vmux* m_vmux;
    Vmux* m_vmux_motion;
#ifndef NO_ISD_AUDIO
    amux_info_t m_audioInfo;
    Amux* m_amux;
    bool m_audioEnabled;
    nxcip::CompressionType m_audioCodec;
    std::unique_ptr<nxcip::AudioFormat> m_audioFormat;
#endif
    mutable std::mutex m_mutex;
    
    vmux_stream_info_t motion_stream_info;
    ISDMotionEstimation m_motionEstimation;
    int64_t m_firstFrameTime;
    unsigned int m_prevPts;
    int64_t m_ptsDelta;
    QnRtspTimeHelper m_timeHelper;
    RtspStatistic m_timeStatistics;

    int m_epollFD;
#ifdef DUPLICATE_MOTION_TO_HIGH_STREAM
    MotionDataPicture* m_currentMotionData;
#endif

    int initializeVMux();
    int getVideoPacket( nxcip::MediaDataPacket** packet );
    bool registerFD( int fd );
    void unregisterFD( int fd );
#ifndef NO_ISD_AUDIO
    int initializeAMux();
    int getAudioPacket( nxcip::MediaDataPacket** packet );
    void fillAudioFormat( const ISDAudioPacket& audioPacket );
#endif
    int64_t calcNextTimestamp( unsigned int pts );
};

#endif  //ILP_STREAM_READER_H
