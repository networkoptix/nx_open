/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_ARCHIVE_READER_H
#define ILP_ARCHIVE_READER_H

#include <memory>
#include <string>

#include <plugins/camera_plugin.h>

#include <plugins/plugin_tools.h>
#include "stream_reader.h"


class DirContentsManager;

class ArchiveReader
:
    public nxcip::DtsArchiveReader
{
public:
    ArchiveReader(
        DirContentsManager* const dirContentsManager,
        unsigned int frameDurationUsec );
    virtual ~ArchiveReader();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::DtsArchiveReader::getCapabilities
    virtual unsigned int getCapabilities() const override;
    //!Implementation of nxcip::DtsArchiveReader::open
    virtual int open() override;
    //!Implementation of nxcip::DtsArchiveReader::getStreamReader
    virtual nxcip::StreamReader* getStreamReader() override;
    //!Implementation of nxcip::DtsArchiveReader::startTime
    virtual nxcip::UsecUTCTimestamp startTime() const override;
    //!Implementation of nxcip::DtsArchiveReader::endTime
    virtual nxcip::UsecUTCTimestamp endTime() const override;
    //!Implementation of nxcip::DtsArchiveReader::seek
    virtual int seek(
        unsigned int cSeq,
        nxcip::UsecUTCTimestamp timestamp,
        bool findKeyFrame,
        nxcip::UsecUTCTimestamp* selectedPosition ) override;
    //!Implementation of nxcip::DtsArchiveReader::toggleReverseMode
    virtual int setReverseMode(
        unsigned int cSeq,
        bool isReverse,
        nxcip::UsecUTCTimestamp timestamp,
        nxcip::UsecUTCTimestamp* selectedPosition ) override;
    //!Implementation of nxcip::DtsArchiveReader::isReverseModeEnabled
    virtual bool isReverseModeEnabled() const override;
    //!Implementation of nxcip::DtsArchiveReader::toggleMotionData
    virtual int setMotionDataEnabled( bool motionPresent ) override;
    //!Implementation of nxcip::DtsArchiveReader::setQuality
    virtual int setQuality( nxcip::MediaStreamQuality quality, bool waitForKeyFrame ) override;
    //!Implementation of nxcip::DtsArchiveReader::setSkipFrames
    virtual int playRange(
        unsigned int cSeq,
        nxcip::UsecUTCTimestamp start,
        nxcip::UsecUTCTimestamp endTimeHint,
        unsigned int step ) override;
    //!Implementation of nxcip::DtsArchiveReader::getLastErrorString
    virtual void getLastErrorString( char* errorString ) const override;

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<StreamReader> m_streamReader;
    DirContentsManager* const m_dirContentsManager;
};

#endif  //ILP_ARCHIVE_READER_H
