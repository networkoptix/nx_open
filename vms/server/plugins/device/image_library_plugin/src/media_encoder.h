// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "stream_reader.h"

class CameraManager;

/*!
    \note Delegates reference counting to \a AxisCameraManager instance
*/
class MediaEncoder
:
    public nxcip::CameraMediaEncoder2
{
public:
    MediaEncoder(
        CameraManager* const cameraManager,
        int encoderNumber,
        unsigned int frameDurationUsec );
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

    //!Implementation of nxcip::CameraMediaEncoder::setBitrate
    virtual nxcip::StreamReader* getLiveStreamReader() override;
    virtual int getAudioFormat( nxcip::AudioFormat* format ) const override;

private:
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    std::unique_ptr<StreamReader> m_streamReader;
    unsigned int m_frameDurationUsec;
    int m_encoderNumber;
};
