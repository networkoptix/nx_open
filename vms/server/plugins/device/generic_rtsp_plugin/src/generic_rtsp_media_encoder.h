/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#pragma once

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>


class GenericRTSPCameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class GenericRTSPMediaEncoder: public nxcip::CameraMediaEncoder4
{
public:
    GenericRTSPMediaEncoder(GenericRTSPCameraManager* const cameraManager, int encoderIndex);
    virtual ~GenericRTSPMediaEncoder();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

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


    //!Implementation of nxcip::CameraMediaEncoder2::getLiveStreamReader
    virtual nxcip::StreamReader* getLiveStreamReader() override;

    //!Implementation of nxcip::CameraMediaEncoder3::getConfiguredLiveStreamReader
    virtual int getConfiguredLiveStreamReader(
        nxcip::LiveStreamConfig* config, nxcip::StreamReader** reader) override;
    //!Implementation of nxcip::CameraMediaEncoder2::getAudioFormat
    virtual int getAudioFormat(nxcip::AudioFormat* audioFormat) const override;
    //!Implementation of nxcip::CameraMediaEncoder3::getVideoFormat
    virtual int getVideoFormat(
        nxcip::CompressionType* codec, nxcip::PixelFormat* pixelFormat) const override;

    //!Implementation of nxcip::CameraMediaEncoder4::setMediaUrl
    virtual int setMediaUrl(const char url[nxcip::MAX_TEXT_LEN]) override;

private:
    nxpt::CommonRefManager m_refManager;
    GenericRTSPCameraManager* m_cameraManager;
    int m_encoderIndex = 0;
    char m_mediaUrl[nxcip::MAX_TEXT_LEN];
};
