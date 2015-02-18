#ifndef ITE_MEDIA_ENCODER_H
#define ITE_MEDIA_ENCODER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"


namespace ite
{
    ///
    class MediaEncoder : public nxcip::CameraMediaEncoder2, public ObjectCounter<MediaEncoder>
    {
        DEF_REF_COUNTER

    public:
        MediaEncoder( CameraManager* cameraManager, int encoderNumber, const nxcip::ResolutionInfo& );
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

        //static void fakeFree(MediaEncoder * ) {}
        void updateResolution(const nxcip::ResolutionInfo& res) { m_resolution = res; }

    private:
        CameraManager * m_cameraManager;
        int m_encoderNumber;
        nxcip::ResolutionInfo m_resolution;
    };
}

#endif // ITE_MEDIA_ENCODER_H
