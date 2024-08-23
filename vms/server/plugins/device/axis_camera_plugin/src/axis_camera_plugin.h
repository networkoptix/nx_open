// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QThread>
#include <QtNetwork/QNetworkAccessManager>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

class AxisCameraDiscoveryManager;

/**
 * The main plugin class. Hosts and initializes the necessary internal data.
 *
 * This project demonstrates usage of camera integration plugin API to add AXIS camera support to
 * the Server. Overrides built-in AXIS camera driver.
 *
 * Implements following camera integration interfaces:
 * - nxcip::CameraDiscoveryManager to enable AXIS camera discovery by MDNS.
 * - nxcip::BaseCameraManager to retrieve camera properties and access other managers.
 * - nxcip::CameraMediaEncoder to receive media stream from camera.
 * - nxcip::CameraRelayIOManager to receive relay input port change state events and change relay
 *     output port state.
 *
 * Object life-time management:
 * - All classes implementing `nxcip::` interfaces delegate reference counting (by using
 *     CommonRefManager(CommonRefManager*) constructor) to factory class instance (e.g.
 *     AxisCameraManager is a factory for AxisRelayIOManager, AxisMediaEncoder, etc.).
 * - All factory classes (except for AxisCameraDiscoveryManager) hold a pointer to the child class
 *     object (e.g. AxisRelayIOManager is a child for AxisCameraManager) and delete all children on
 *     destruction.
 */
class AxisCameraPlugin
:
    public nxpl::PluginInterface
{
public:
    AxisCameraPlugin();
    virtual ~AxisCameraPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    nxpt::CommonRefManager* refManager();
    QNetworkAccessManager* networkAccessManager();

    static AxisCameraPlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<AxisCameraDiscoveryManager> m_discoveryManager;
    //!Used with QNetworkAccessManager
    QThread m_networkEventLoopThread;
    QNetworkAccessManager m_networkAccessManager;
};
