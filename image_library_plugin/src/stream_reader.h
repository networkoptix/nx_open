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

#include "common_ref_manager.h"
#include "mutex.h"


class DirContentsManager;

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
        CommonRefManager* const parentRefManager,
        const DirContentsManager& dirContentsManager,
        unsigned int frameDurationUsec,
        bool liveMode );
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

    nxcip::UsecUTCTimestamp setPosition( nxcip::UsecUTCTimestamp timestamp );

private:
    CommonRefManager m_refManager;
    const DirContentsManager& m_dirContentsManager;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    const unsigned int m_frameDuration;
    bool m_liveMode;
    std::map<nxcip::UsecUTCTimestamp, std::string> m_dirEntries;
    std::map<nxcip::UsecUTCTimestamp, std::string>::const_iterator m_curPos;
    bool m_streamReset;
    nxcip::UsecUTCTimestamp m_nextFrameDeployTime;
    mutable Mutex m_mutex;

    void doLiveDelay();
    void readDirContents();
    void moveCursorToNextFrame();
};

#endif  //ILP_STREAM_READER_H
