/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_MEDIA_ENCODER_H
#define AXIS_MEDIA_ENCODER_H

#include <vector>

#include <QAtomicInt>

#include <plugins/camera_plugin.h>


class AxisCameraManager;

class AxisMediaEncoder
:
    public nxcip::CameraMediaEncoder
{
public:
    AxisMediaEncoder( AxisCameraManager* const cameraManager );

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

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
    //!Implementation of nxcip::CameraMediaEncoder::getBaseCameraManager
    //virtual nxcip::BaseCameraManager* getBaseCameraManager();

private:
    QAtomicInt m_refCount;
    AxisCameraManager* m_cameraManager;
    mutable std::vector<nxcip::ResolutionInfo> m_supportedResolutions;
    nxcip::ResolutionInfo m_currentResolutionInfo;
    float m_currentFps;
    int m_currentBitrateKbps;
    bool m_audioEnabled;
    float m_maxAllowedFps;

    int fetchCameraResolutionList() const;
};

#endif  //AXIS_MEDIA_ENCODER_H
