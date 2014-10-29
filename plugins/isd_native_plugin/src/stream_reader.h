/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <atomic>
#include <memory>
#include <stdint.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include <isd/vmux/vmux_iface.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <utils/media/pts_to_clock_mapper.h>
#include <utils/memory/cyclic_allocator.h>

#include "audio_stream_reader.h"
#include "isd_motion_estimation.h"
#include "waitable_queue_with_eventfd.h"

//#define DEBUG_OUTPUT


class ISDAudioPacket;
class MotionDataPicture;

//!Reads picture files from specified directory as video-stream
class StreamReader
:
    public nxcip::StreamReader
{
public:
    /*!
        \param audioStreamBridge Used to copy audio packets from primary stream reader to a secondary one. 
            This is needed since audio can be read be single stream reader only but both readers must 
            provide audio stream.
        \note \a StreamReader instance with \a encoderNum set to 0 pushes packets to \a audioStreamBridge. 
            The other \a StreamReader instance pops packets
    */
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderNum,
#ifndef NO_ISD_AUDIO
        const std::unique_ptr<AudioStreamReader>& audioStreamReader,
#endif
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
    //int getAudioFormat( nxcip::AudioFormat* audioFormat ) const;

private:
    nxpt::CommonRefManager m_refManager;
    int m_encoderNum;
    nxcip::CompressionType m_videoCodec;
    nxcip::UsecUTCTimestamp m_lastVideoTime;
    nxcip::UsecUTCTimestamp m_lastMotionTime;
    std::unique_ptr<Vmux> m_vmux;
    std::unique_ptr<Vmux> m_vmuxMotion;
#ifndef NO_ISD_AUDIO
    const std::unique_ptr<AudioStreamReader>& m_audioStreamReader;
    std::deque<std::shared_ptr<AudioData>> m_audioPackets;
    int m_audioEventFD;
    bool m_audioEventFDRegistered;
    //amux_info_t m_audioInfo;
    //std::unique_ptr<Amux> m_amux;
    std::atomic<bool> m_audioEnabled;
    //nxcip::CompressionType m_audioCodec;
    //std::unique_ptr<nxcip::AudioFormat> m_audioFormat;
    size_t m_audioReceiverID;
#endif
    mutable QMutex m_mutex;
    
    vmux_stream_info_t motion_stream_info;
    ISDMotionEstimation m_motionEstimation;
    int64_t m_currentTimestamp;
    unsigned int m_prevPts;
    int64_t m_timestampDelta;
    int m_framesSinceTimeResync;
    int m_epollFD;
    MotionDataPicture* m_motionData;
    QAtomicInt m_refCounter;

    PtsToClockMapper m_ptsMapper;
    CyclicAllocator m_allocator;
    size_t m_currentGopSizeBytes;

#ifdef DEBUG_OUTPUT
    QElapsedTimer m_frameTimer;
    size_t m_totalFramesRead;
#endif

    int initializeVMux();
    int initializeVMuxMotion();
    int getVideoPacket( nxcip::MediaDataPacket** packet );
    bool needMetaData();
    void readMotion();
    MotionDataPicture* getMotionData();
    bool registerFD( int fd );
    void unregisterFD( int fd );
#ifndef NO_ISD_AUDIO
    //int initializeAMux();
    void onAudioDataAvailable( const std::shared_ptr<AudioData>& audioDataPtr );
    int getAudioPacket( nxcip::MediaDataPacket** packet );
    //void fillAudioFormat( const ISDAudioPacket& audioPacket );
#endif
    void closeAllStreams();
    int64_t calcNextTimestamp( int32_t pts, int64_t absoluteTimeMS );
    void resyncTime( int64_t absoluteSourceTimeUSec );
};

#endif  //ILP_STREAM_READER_H
