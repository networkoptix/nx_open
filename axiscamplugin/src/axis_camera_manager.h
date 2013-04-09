/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_CAMERA_MANAGER_H
#define AXIS_CAMERA_MANAGER_H

#include <vector>

#include <QAtomicInt>
#include <QAuthenticator>
#include <QString>

#include <plugins/camera_plugin.h>


class AxisMediaEncoder;

class AxisCameraManager
:
    public nxcip::BaseCameraManager
{
public:
    AxisCameraManager( const nxcip::CameraInfo& info );
    virtual ~AxisCameraManager();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::BaseCameraManager::getEncoderCount
    virtual int getEncoderCount( int* encoderCount ) const override;
    //!Implementation of nxcip::BaseCameraManager::getEncoder
    virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr );
    //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
    virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
    //!Implementation of nxcip::BaseCameraManager::setCredentials
    virtual void setCredentials( const char* username, const char* password ) override;
    //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
    virtual int setAudioEnabled( int audioEnabled ) override;
    //!Implementation of nxcip::BaseCameraManager::getPTZManager
    virtual nxcip::CameraPTZManager* getPTZManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getErrorString
    virtual void getErrorString( int errorCode, char* errorString ) const override;

    const nxcip::CameraInfo& cameraInfo() const;
    const QAuthenticator& credentials() const;

    bool isAudioEnabled() const;

private:
    QAtomicInt m_refCount;
    const nxcip::CameraInfo m_info;
    const QString m_managementURL;
    QAuthenticator m_credentials;
    mutable std::vector<AxisMediaEncoder*> m_encoders;
    bool m_audioEnabled;
};

#endif  //AXIS_CAMERA_MANAGER_H
