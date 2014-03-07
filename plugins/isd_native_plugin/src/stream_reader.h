/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <stdint.h>

#include <memory>
#include <mutex>

#include <isd/amux/amux_iface.h>
#include <isd/vmux/vmux_iface.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "isd_motion_estimation.h"


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
    StreamReader( nxpt::CommonRefManager* const parentRefManager, int encoderNum);
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
    amux_info_t m_audioInfo;
    Amux* m_amux;
    nxcip::CompressionType m_audioCodec;
    mutable std::mutex m_mutex;
    std::unique_ptr<nxcip::AudioFormat> m_audioFormat;
    
    vmux_stream_info_t motion_stream_info;
    ISDMotionEstimation m_motionEstimation;
    int64_t m_firstFrameTime;
    unsigned int m_prevPts;
    int64_t m_ptsDelta;
    bool m_audioEnabled;

    int m_epollFD;

    int initializeVMux();
    int initializeAMux();
    int getVideoPacket( nxcip::MediaDataPacket** packet );
    int getAudioPacket( nxcip::MediaDataPacket** packet );
    bool registerFD( int fd );
    void unregisterFD( int fd );
    void fillAudioFormat( const ISDAudioPacket& audioPacket );
    int64_t calcNextTimestamp( unsigned int pts );
};

#endif  //ILP_STREAM_READER_H
