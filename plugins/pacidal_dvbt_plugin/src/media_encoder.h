#ifndef PACIDAL_MEDIA_ENCODER_H
#define PACIDAL_MEDIA_ENCODER_H

#include <memory>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "stream_reader.h"


namespace pacidal
{
    class LibAV;

    /*!
        \note Delegates reference counting to \a AxisCameraManager instance
    */
    class MediaEncoder
    :
        public nxcip::CameraMediaEncoder2
    {
        DEF_REF_COUNTER

    public:
        MediaEncoder( CameraManager* cameraManager, int encoderNumber );
        virtual ~MediaEncoder();

        //!Implementation of nxcip::CameraMediaEncoder::getMediaUrl
        virtual int getMediaUrl( char* urlBuf ) const override;
        //!Implementation of nxcip::CameraMediaEncoder::getResolutionList
        virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
        //!Implementation of nxcip::CameraMediaEncoder::getMaxBitrate
        virtual int getMaxBitrate( int* maxBitrate ) const override;
        //!Implementation of nxcip::CameraMediaEncoder::setResolution
        virtual int setResolution( const nxcip::Resolution& resolution ) override;
        //!Implementation of nxcip::CameraMediaEncoder::setFps
        virtual int setFps(const float&, float* selectedFps ) override;
        //!Implementation of nxcip::CameraMediaEncoder::setBitrate
        virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

        //!Implementation of nxcip::CameraMediaEncoder2::getLiveStreamReader
        virtual nxcip::StreamReader* getLiveStreamReader() override;
        //!Implementation of nxcip::CameraMediaEncoder2::getAudioFormat
        virtual int getAudioFormat( nxcip::AudioFormat* audioFormat ) const override;

    private:
        CameraManager* m_cameraManager;
        std::unique_ptr<StreamReader> m_streamReader;
        int m_encoderNumber;
    };
}

#endif  //PACIDAL_MEDIA_ENCODER_H
