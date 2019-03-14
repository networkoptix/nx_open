#pragma once

#include <memory>

#include <QtCore/QSize>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "stream_reader.h"

namespace nx::vms_server_plugins::mjpeg_link {

class CameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class MediaEncoder: public nxcip::CameraMediaEncoder4
{
public:
    MediaEncoder(
        CameraManager* const cameraManager,
        nxpl::TimeProvider *const timeProvider,
        int encoderNumber );
    virtual ~MediaEncoder();

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

    //!Implementation of nxcip::CameraMediaEncoder2::setBitrate
    virtual nxcip::StreamReader* getLiveStreamReader() override;
    //!Implementation of nxcip::CameraMediaEncoder2::getAudioFormat
    virtual int getAudioFormat( nxcip::AudioFormat* audioFormat ) const override;

    void updateCredentials(const QString& login, const QString& password);

    //!Implementation of nxcip::CameraMediaEncoder3::getConfiguredLiveStreamReader
    virtual int getConfiguredLiveStreamReader(
        nxcip::LiveStreamConfig* config, nxcip::StreamReader** reader) override;
    //!Implementation of nxcip::CameraMediaEncoder3::getVideoFormat
    virtual int getVideoFormat(
        nxcip::CompressionType* codec, nxcip::PixelFormat* pixelFormat) const override;

    //!Implementation of nxcip::CameraMediaEncoder4::setMediaUrl
    virtual int setMediaUrl(const char url[nxcip::MAX_TEXT_LEN]) override;
private:
    QString getMediaUrlInternal() const;
private:
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    nxpl::TimeProvider *const m_timeProvider;
    std::unique_ptr<StreamReader> m_streamReader;
    int m_encoderNumber;
    QSize m_resolution;
    float m_currentFps;
    QString m_mediaUrl;
};

} // namespace nx::vms_server_plugins::mjpeg_link
