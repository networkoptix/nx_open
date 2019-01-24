/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <stdint.h>

#include <map>
#include <string>

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>
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
        nxpt::CommonRefManager* const parentRefManager,
        DirContentsManager* const dirContentsManager,
        unsigned int frameDurationUsec,
        bool liveMode,
        int encoderNumber );
    virtual ~StreamReader();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    //!Implementation nxcip::StreamReader::getNextData
    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    //!Implementation nxcip::StreamReader::interrupt
    virtual void interrupt() override;

    nxcip::UsecUTCTimestamp setPosition(
        unsigned int cSeq,
        nxcip::UsecUTCTimestamp timestamp );
    /*!
        \return Actually selected timestamp
    */
    nxcip::UsecUTCTimestamp setReverseMode(
        unsigned int cSeq,
        bool isReverse,
        nxcip::UsecUTCTimestamp timestamp );
    bool isReverse() const;

private:
    nxpt::CommonRefManager m_refManager;
    DirContentsManager* const m_dirContentsManager;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    const unsigned int m_frameDuration;
    bool m_liveMode;
    int m_encoderNumber;
    std::map<nxcip::UsecUTCTimestamp, std::string> m_dirEntries;
    std::map<nxcip::UsecUTCTimestamp, std::string>::const_iterator m_curPos;
    bool m_streamReset;
    nxcip::UsecUTCTimestamp m_nextFrameDeployTime;
    mutable Mutex m_mutex;
    bool m_isReverse;
    unsigned int m_cSeq;

    void doLiveDelay();
    void readDirContents();
    void moveCursorToNextFrame();
};

#endif  //ILP_STREAM_READER_H
