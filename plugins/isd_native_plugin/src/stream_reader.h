/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <stdint.h>

#include <map>
#include <string>

#include <plugins/camera_plugin.h>

#include <plugins/plugin_tools.h>
#include "mutex.h"
#include <isd/vmux/vmux_iface.h>
#include "isd_motion_estimation.h"

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

    void setMotionMask(const uint8_t* data);
private:
    bool StreamReader::needMetaData();
    MotionDataPicture* getMotionData();
private:
    nxpt::CommonRefManager m_refManager;
    int m_encoderNum;
    nxcip::CompressionType m_codec;
    nxcip::UsecUTCTimestamp m_lastVideoTime;
    nxcip::UsecUTCTimestamp m_lastMotionTime;
    Vmux* vmux;
    
    Vmux* vmux_motion;
    vmux_stream_info_t motion_stream_info;
    ISDMotionEstimation m_motionEstimation;
};

#endif  //ILP_STREAM_READER_H
