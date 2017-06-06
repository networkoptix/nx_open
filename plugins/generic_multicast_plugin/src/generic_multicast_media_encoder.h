#pragma once

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>


class GenericMulticastCameraManager;
class GenericMulticastStreamReader;

class GenericMulticastMediaEncoder: public nxcip::CameraMediaEncoder2
{

public:
    GenericMulticastMediaEncoder(GenericMulticastCameraManager* const cameraManager);
    virtual ~GenericMulticastMediaEncoder();

    /** Implementation of nxpl::PluginInterface::queryInterface */
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    /** Implementation of nxpl::PluginInterface::addRef */
    virtual unsigned int addRef() override;
    /** Implementation of nxpl::PluginInterface::releaseRef */
    virtual unsigned int releaseRef() override;

    /** Implementation of nxcip::CameraMediaEncoder::getMediaUrl */
    virtual int getMediaUrl( char* urlBuf ) const override;
    /** Implementation of nxcip::CameraMediaEncoder::getResolutionList */
    virtual int getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const override;
    /** Implementation of nxcip::CameraMediaEncoder::getMaxBitrate */
    virtual int getMaxBitrate( int* maxBitrate ) const override;
    /** Implementation of nxcip::CameraMediaEncoder::setResolution */
    virtual int setResolution( const nxcip::Resolution& resolution ) override;
    /** Implementation of nxcip::CameraMediaEncoder::setFps */
    virtual int setFps( const float& fps, float* selectedFps ) override;
    /** Implementation of nxcip::CameraMediaEncoder::setBitrate */
    virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) override;

    /** Implemantation of nxcip::CameraMediaEncoder2::getLiveStreamReader */
    virtual nxcip::StreamReader* getLiveStreamReader() override;

    /** Implemantation of nxcip::CameraMediaEncoder4::getAudioFormat */
    virtual int getAudioFormat(nxcip::AudioFormat* audioFormat) const override;

private:
    nxpt::CommonRefManager m_refManager;
    GenericMulticastCameraManager* m_cameraManager;
    std::unique_ptr<GenericMulticastStreamReader> m_streamReader;
};
