/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_ARCHIVE_READER_H
#define ILP_ARCHIVE_READER_H

#include <memory>
#include <string>

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"
#include "stream_reader.h"


class ArchiveReader
:
    public nxcip::DtsArchiveReader
{
public:
    ArchiveReader( const std::string& pictureDirectoryPath );
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
    virtual bool open() override;
    //!Implementation of nxcip::DtsArchiveReader::getStreamReader
    virtual nxcip::StreamReader* getStreamReader() override;
    //!Implementation of nxcip::DtsArchiveReader::startTime
    virtual nxcip::UsecUTCTimestamp startTime() const override;
    //!Implementation of nxcip::DtsArchiveReader::endTime
    virtual nxcip::UsecUTCTimestamp endTime() const override;
    //!Implementation of nxcip::DtsArchiveReader::seek
    virtual int seek( nxcip::UsecUTCTimestamp timestamp, bool findKeyFrame, nxcip::UsecUTCTimestamp* selectedPosition ) override;
    //!Implementation of nxcip::DtsArchiveReader::toggleReverseMode
    virtual int setReverseMode( bool isReverse, nxcip::UsecUTCTimestamp timestamp ) override;
    //!Implementation of nxcip::DtsArchiveReader::toggleMotionData
    virtual int setMotionDataEnabled( bool motionPresent ) override;
    //!Implementation of nxcip::DtsArchiveReader::setQuality
    virtual int setQuality( nxcip::MediaStreamQuality quality, bool waitForKeyFrame ) override;
    //!Implementation of nxcip::DtsArchiveReader::setSkipFrames
    virtual int setSkipFrames( nxcip::UsecUTCTimestamp step ) override;
    //!Implementation of nxcip::DtsArchiveReader::find
    virtual int find( nxcip::Picture* motionMask, nxcip::TimePeriods** timePeriods ) override;
    //!Implementation of nxcip::DtsArchiveReader::getLastErrorString
    virtual void getLastErrorString( char* errorString ) const override;

private:
    CommonRefManager m_refManager;
    std::string m_pictureDirectoryPath;
    std::auto_ptr<StreamReader> m_streamReader;
};

#endif  //ILP_ARCHIVE_READER_H
