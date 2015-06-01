#ifndef RPI_MEDIA_ENCODER_H
#define RPI_MEDIA_ENCODER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"


namespace rpi_cam
{
    class RPiCamera;

    //!
    class MediaEncoder : public nxcip::CameraMediaEncoder2
    {
        DEF_REF_COUNTER

    public:
        MediaEncoder(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber, const nxcip::ResolutionInfo& resolution);
        virtual ~MediaEncoder();

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

        //

        virtual int commit(); // override

    private:
        std::shared_ptr<RPiCamera> m_camera;
        unsigned m_encoderNumber;
        nxcip::ResolutionInfo m_resolution;
    };
}

#endif
