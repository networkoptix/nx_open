#pragma once

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "stream_reader.h"
#include "device_data.h"
#include "codec_context.h"

namespace nx {
namespace webcam_plugin {

class CameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class MediaEncoder
:
    public nxcip::CameraMediaEncoder2
{
public:
    MediaEncoder(CameraManager* const cameraManager, 
                 nxpl::TimeProvider *const timeProvider,
                 int encoderNumber,
                 const CodecContext& codecContext);
    virtual ~MediaEncoder();

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

    //!Implementation of nxcip::CameraMediaEncoder2::setBitrate
    virtual nxcip::StreamReader* getLiveStreamReader() override;
    //!Implementation of nxcip::CameraMediaEncoder2::getAudioFormat
    virtual int getAudioFormat( nxcip::AudioFormat* audioFormat ) const override;

    void updateCameraInfo( const nxcip::CameraInfo& info );
    void setVideoCodecID(nxcip::CompressionType codecID);

private:
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    nxpl::TimeProvider *const m_timeProvider;
    std::unique_ptr<StreamReader> m_streamReader;
    int m_encoderNumber;
    CodecContext m_videoCodecContext;
    mutable int m_maxBitrate;
private:
    QString decodeCameraInfoUrl() const;
};

} // namespace nx 
} // namespace webcam_plugin 
