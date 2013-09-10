/**********************************************************
* 03 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_MEDIA_ENCODER_H
#define ILP_MEDIA_ENCODER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "common_ref_manager.h"
#include "stream_reader.h"


class CameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class MediaEncoder
:
    public nxcip::CameraMediaEncoder2
{
public:
    MediaEncoder(
        CameraManager* const cameraManager,
        unsigned int frameDurationUsec );
    virtual ~MediaEncoder();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::CameraMediaEncoder::getMediaUrl
    virtual int getMediaUrl( char* urlBuf ) const override;
    //!Implementation of nxcip::CameraMediaEncoder::getResolutionList
    virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
    //!Implementation of nxcip::CameraMediaEncoder::getMaxBitrate
    virtual int getMaxBitrate( int* maxBitrate ) const override;
    //!Implementation of nxcip::CameraMediaEncoder::setResolution
    virtual int setResolution( const nxcip::Resolution& resolution ) override;
    //!Implementation of nxcip::CameraMediaEncoder::setFps
    virtual int setFps( const float& fps, float* selectedFps ) override;
    //!Implementation of nxcip::CameraMediaEncoder::setBitrate
    virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

    //!Implementation of nxcip::CameraMediaEncoder::setBitrate
    virtual nxcip::StreamReader* getLiveStreamReader() override;

private:
    CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    std::auto_ptr<StreamReader> m_streamReader;
    unsigned int m_frameDurationUsec;
};

#endif  //ILP_MEDIA_ENCODER_H
