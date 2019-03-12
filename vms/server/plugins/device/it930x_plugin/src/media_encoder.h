#ifndef ITE_MEDIA_ENCODER_H
#define ITE_MEDIA_ENCODER_H

#include <memory>

#include <camera/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"


namespace ite
{
    ///
    class MediaEncoder : public DefaultRefCounter<nxcip::CameraMediaEncoder3>
    {
    public:
        MediaEncoder( CameraManager* cameraManager, int encoderNumber );
        virtual ~MediaEncoder();

        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

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

        virtual int getConfiguredLiveStreamReader(nxcip::LiveStreamConfig * , nxcip::StreamReader ** ) override;
        virtual int getVideoFormat(nxcip::CompressionType * codec, nxcip::PixelFormat * pixelFormat) const override;

    private:
        CameraManager * m_cameraManager;
        int m_encoderNumber;
    };
}

#endif // ITE_MEDIA_ENCODER_H
