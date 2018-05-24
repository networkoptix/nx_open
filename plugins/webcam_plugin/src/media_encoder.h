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

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getMediaUrl( char* urlBuf ) const override;
    virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
    virtual int getMaxBitrate( int* maxBitrate ) const override;
    virtual int setResolution( const nxcip::Resolution& resolution ) override;
    virtual int setFps( const float& fps, float* selectedFps ) override;
    virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

    virtual nxcip::StreamReader* getLiveStreamReader() override;
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
