/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_CAMERA_MANAGER_H
#define AXIS_CAMERA_MANAGER_H

#include <memory>
#include <vector>

#include <QtNetwork/QAuthenticator>
#include <QtCore/QString>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include <plugins/plugin_tools.h>


class AxisCameraPlugin;
class AxisMediaEncoder;
class AxisRelayIOManager;
class SyncHttpClient;

//!Provides access to camera's properties and instanciates other managers (implements \a nxcip::BaseCameraManager)
class AxisCameraManager
:
    public nxcip::BaseCameraManager
{
public:
    AxisCameraManager( const nxcip::CameraInfo& info );
    virtual ~AxisCameraManager();

    //!Implementaion of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation of nxcip::BaseCameraManager::getEncoderCount
    virtual int getEncoderCount( int* encoderCount ) const override;
    //!Implementation of nxcip::BaseCameraManager::getEncoder
    virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
    //!Implementation of nxcip::BaseCameraManager::getCameraInfo
    virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
    virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
    //!Implementation of nxcip::BaseCameraManager::setCredentials
    virtual void setCredentials( const char* username, const char* password ) override;
    //!Implementation of nxcip::BaseCameraManager::setAudioEnabled
    virtual int setAudioEnabled( int audioEnabled ) override;
    //!Implementation of nxcip::BaseCameraManager::getPTZManager
    virtual nxcip::CameraPtzManager* getPtzManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
    virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
    //!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
    virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
    //!Implementation of nxcip::BaseCameraManager::getLastErrorString
    virtual void getLastErrorString( char* errorString ) const override;

    const nxcip::CameraInfo& cameraInfo() const;
    nxcip::CameraInfo& cameraInfo();
    const QAuthenticator& credentials() const;

    bool isAudioEnabled() const;

    //!reads axis parameter, triggering url like http://ip/axis-cgi/param.cgi?action=list&group=Input.NbrOfInputs
    static int readAxisParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QVariant* paramValue );
    static int readAxisParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QByteArray* paramValue );
    static int readAxisParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        QString* paramValue );
    static int readAxisParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        unsigned int* paramValue );
    static int readAxisParameter(
        SyncHttpClient* const httpClient,
        const QByteArray& paramName,
        int* paramValue );

    nxpt::CommonRefManager* refManager();

private:
    nxpt::CommonRefManager m_refManager;
    /*!
        Holding reference to \a AxisCameraPlugin, but not \a AxisCameraDiscoveryManager, 
        since \a AxisCameraDiscoveryManager instance is not required for \a AxisCameraManager object
    */
    nxpt::ScopedRef<AxisCameraPlugin> m_pluginRef;
    mutable nxcip::CameraInfo m_info;
    const QString m_managementURL;
    QAuthenticator m_credentials;
    mutable std::vector<AxisMediaEncoder*> m_encoders;
    bool m_audioEnabled;
    mutable bool m_relayIOInfoRead;
    mutable std::auto_ptr<AxisRelayIOManager> m_relayIOManager;
    mutable unsigned int m_cameraCapabilities;
    mutable unsigned int m_inputPortCount;
    mutable unsigned int m_outputPortCount;

    int updateCameraInfo() const;
};

#endif  //AXIS_CAMERA_MANAGER_H
