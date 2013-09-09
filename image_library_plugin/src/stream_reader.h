/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <list>
#include <string>

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"
#include "dir_iterator.h"


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
        const std::string& imageDirectoryPath,
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

    nxcip::UsecUTCTimestamp minTimestamp() const;
    nxcip::UsecUTCTimestamp maxTimestamp() const;

private:
    class DirEntry
    {
    public:
        std::string path;
        uint64_t size;

        DirEntry()
        :
            size( 0 )
        {
        }

        DirEntry( const std::string& _path, uint64_t _size )
        :
            path( _path ),
            size( _size )
        {
        }
    };

    CommonRefManager m_refManager;
    const std::string m_imageDirectoryPath;
    DirIterator m_dirIterator;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    unsigned int m_frameDuration;
    bool m_liveMode;
    std::list<DirEntry> m_dirEntries;
    std::list<DirEntry>::const_iterator m_curPos;
    nxcip::UsecUTCTimestamp m_lastPicTimestamp;
    bool m_streamReset;
    nxcip::UsecUTCTimestamp m_nextFrameDeployTime;

    void doLiveDelay();
};

#endif  //ILP_STREAM_READER_H
