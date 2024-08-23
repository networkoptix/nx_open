// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_MEDIA_ENCODER_H
#define RPI_MEDIA_ENCODER_H

#include <memory>

#include <camera/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"

namespace rpi_cam
{
    class RPiCamera;

    //!
    class MediaEncoder : public DefaultRefCounter<nxcip::CameraMediaEncoder3>
    {
    public:
        MediaEncoder(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber);
        virtual ~MediaEncoder();

        // nxpl::PluginInterface

        virtual void * queryInterface( const nxpl::NX_GUID& interfaceID ) override;

        // nxcip::CameraMediaEncoder

        virtual int getMediaUrl( char* urlBuf ) const override;
        virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
        virtual int getMaxBitrate( int* maxBitrate ) const override;
        virtual int setResolution( const nxcip::Resolution& resolution ) override;
        virtual int setFps( const float&, float* selectedFps ) override;
        virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

        // nxcip::CameraMediaEncoder2

        virtual nxcip::StreamReader* getLiveStreamReader() override;
        virtual int getAudioFormat( nxcip::AudioFormat* audioFormat ) const override;

        // nxcip::CameraMediaEncoder3

        virtual int getConfiguredLiveStreamReader(nxcip::LiveStreamConfig * config, nxcip::StreamReader ** reader) override;
        virtual int getVideoFormat(nxcip::CompressionType * codec, nxcip::PixelFormat * pixelFormat) const override;

    private:
        std::weak_ptr<RPiCamera> m_camera;
        unsigned m_encoderNumber;
        unsigned m_bitrateKbps;
    };
}

#endif
