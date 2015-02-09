#ifndef RPI_MEDIA_ENCODER_H
#define RPI_MEDIA_ENCODER_H

#include <memory>
#include <vector>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"


namespace rpi_cam
{
    class CameraManager;

    //!
    class MediaEncoder : public nxcip::CameraMediaEncoder2
    {
        DEF_REF_COUNTER

    public:
        MediaEncoder( CameraManager* cameraManager, unsigned encoderNumber, unsigned maxBitrate = 0);
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

        nxpt::CommonRefManager * refManager() { return &m_refManager; }
        nxcip::MediaDataPacket * nextFrame();

        void addResolution(const nxcip::ResolutionInfo& res) { resolutions_.push_back(res); }

    private:
        CameraManager * cameraManager_;
        std::unique_ptr<StreamReader> streamReader_;
        unsigned encoderNumber_;
        unsigned maxBitrate_;
        std::vector<nxcip::ResolutionInfo> resolutions_;
    };
}

#endif
