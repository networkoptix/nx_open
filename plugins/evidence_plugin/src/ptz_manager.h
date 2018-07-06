/**********************************************************
* 22 apr 2014
* akolesnikov
***********************************************************/

#ifndef PTZ_MANAGER_H
#define PTZ_MANAGER_H

#include <QtCore/QByteArray>
#include <QtCore/QList>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>


class CameraManager;

class PtzManager
:
    public nxcip::CameraPtzManager
{
public:
    PtzManager(
        CameraManager* cameraManager,
        const QList<QByteArray>& cameraParamList,
        const QList<QByteArray>& cameraParamOptions );

    //!Implementaion of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    virtual int getCapabilities() const override;
    virtual int continuousMove( double panSpeed, double tiltSpeed, double zoomSpeed ) override;
    virtual int absoluteMove( CoordinateSpace space, double pan, double tilt, double zoom, double speed ) override;
    virtual int getPosition( CoordinateSpace space, double *pan, double *tilt, double *zoom ) override;
    virtual int getLimits( CoordinateSpace space, Limits *limits ) override;
    virtual int getFlip( int *flip ) override;

private:
    nxpt::CommonRefManager m_refManager;
    CameraManager* m_cameraManager;
    int m_capabilities;
    int m_flip;
    nxcip::CameraPtzManager::Limits m_limits;

    void readParameters(
        const QList<QByteArray>& cameraParamList,
        const QList<QByteArray>& cameraParamOptions );
    double toCameraZoomUnits( double val ) const;
    double fromCameraZoomUnits( double val ) const;
};

#endif  //PTZ_MANAGER_H
