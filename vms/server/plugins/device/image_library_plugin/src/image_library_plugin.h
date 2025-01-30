// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

class DiscoveryManager;

/**
 * Main plugin class. Hosts and initializes the necessary internal data.
 *
 * This project demonstrates usage of camera integration plugin API to add support of a camera with
 * a remote archive (e.g. the storage is bound directly to the camera).
 *
 * Usage:
 *
 * To use this plugin, launch client aplication and use manual camera search ("Add Camera(s)..." in
 * media server menu) filling "Camera Address" field with absolute path to local directory,
 * containing jpeg file(s). Specified directory will be found as camera with archive and appear in
 * tree menu.
 *
 * Implements following camera integration interfaces:
 * - nxcip::CameraDiscoveryManager (DiscoveryManager) to find this plugin.
 * - nxcip::BaseCameraManager2 (CameraManager) to retrieve camera properties and access other
 *     managers.
 * - nxcip::CameraMediaEncoder2 (MediaEncoder) to receive LIVE media stream from camera.
 * - nxcip::StreamReader (StreamReader) to provide media stream directly from plugin.
 * - nxcip::DtsArchiveReader (ArchiveReader) to access archive.
 *
 * Object life-time management:
 * - Plugin entry point is createNXPluginInstance function.
 * - All classes implementing `nxcip::` interfaces delegate reference counting (by using
 *     CommonRefManager(CommonRefManager*) constructor) to factory class instance (e.g.
 *     CameraManager is a factory for MediaEncoder). This garantees that CameraManager instance
 *     is removed later than MediaEncoder instance.
 * - All factory classes (except for CameraDiscoveryManager) hold pointer to child class object
 *     (e.g. MediaEncoder is a child for CameraManager) and delete all children on destruction.
 */
class ImageLibraryPlugin
:
    public nxpl::PluginInterface
{
public:
    ImageLibraryPlugin();
    virtual ~ImageLibraryPlugin();

    //!Implementation of nxpl::PluginInterface::queryInterface
    /*!
        Supports cast to nxcip::CameraDiscoveryManager interface
    */
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

    nxpt::CommonRefManager* refManager();

    static ImageLibraryPlugin* instance();

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<DiscoveryManager> m_discoveryManager;
};
