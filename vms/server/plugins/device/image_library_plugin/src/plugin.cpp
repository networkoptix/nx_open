// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin.h"
#include "discovery_manager.h"

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new ImageLibraryPlugin();
    }
}

static ImageLibraryPlugin* imageLibraryPluginInstance = NULL;

ImageLibraryPlugin::ImageLibraryPlugin()
:
    m_refManager( this )
{
    imageLibraryPluginInstance = this;

    m_discoveryManager.reset( new DiscoveryManager() );
}

ImageLibraryPlugin::~ImageLibraryPlugin()
{
    imageLibraryPluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* ImageLibraryPlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return NULL;
}

int ImageLibraryPlugin::addRef() const
{
    return m_refManager.addRef();
}

int ImageLibraryPlugin::releaseRef() const
{
    return m_refManager.releaseRef();
}

nxpt::CommonRefManager* ImageLibraryPlugin::refManager()
{
    return &m_refManager;
}

ImageLibraryPlugin* ImageLibraryPlugin::instance()
{
    return imageLibraryPluginInstance;
}
