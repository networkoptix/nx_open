/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#ifndef GENERIC_RTSP_MEDIA_ENCODER_H
#define GENERIC_RTSP_MEDIA_ENCODER_H

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>


class GenericRTSPCameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class GenericRTSPMediaEncoder
:
    public nxcip::CameraMediaEncoder
{
public:
    GenericRTSPMediaEncoder( GenericRTSPCameraManager* const cameraManager );
    virtual ~GenericRTSPMediaEncoder();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
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

private:
    nxpt::CommonRefManager m_refManager;
    GenericRTSPCameraManager* m_cameraManager;
};

#endif  //GENERIC_RTSP_MEDIA_ENCODER_H
