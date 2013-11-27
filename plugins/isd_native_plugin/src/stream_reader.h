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
#include "vmux_iface.h"

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
private:
    nxpt::CommonRefManager m_refManager;
    int m_encoderNum;
    bool m_initialized;
    nxcip::CompressionType m_codec;

    Vmux vmux;
};

#endif  //ILP_STREAM_READER_H
