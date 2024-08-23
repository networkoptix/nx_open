// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

class AxisCameraManager;

//!Implementation of \a nxcip::CameraMediaEncoder
/*!
    \note Delegates reference counting to \a AxisCameraManager instance (i.e., increments \a AxisCameraManager reference counter on initialiation and decrements on destruction)
*/
class AxisMediaEncoder
:
    public nxcip::CameraMediaEncoder
{
public:
    //!Initialization
    /*!
        \param cameraManager Reference counting is delegated to this object
    */
    AxisMediaEncoder( AxisCameraManager* const cameraManager );
    virtual ~AxisMediaEncoder();

    //!Implementaion of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
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

private:
    nxpt::CommonRefManager m_refManager;
    AxisCameraManager* m_cameraManager;
    mutable std::vector<nxcip::ResolutionInfo> m_supportedResolutions;
    nxcip::ResolutionInfo m_currentResolutionInfo;
    float m_currentFps;
    int m_currentBitrateKbps;
    bool m_audioEnabled;
    float m_maxAllowedFps;
    mutable int m_rtspPort;

    int fetchCameraResolutionList() const;
};
