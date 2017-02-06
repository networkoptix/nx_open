/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "plugin.h"

#include "discovery_manager.h"
#include "settings.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::PluginInterface* createNXPluginInstance()
    {
        return new IsdNativePlugin();
    }
}

static IsdNativePlugin* isdNativePluginInstance = NULL;

IsdNativePlugin::IsdNativePlugin()
:
    m_refManager( this )
{
    isdNativePluginInstance = this;

    m_discoveryManager.reset( new DiscoveryManager() );
}

IsdNativePlugin::~IsdNativePlugin()
{
    isdNativePluginInstance = NULL;
}

//!Implementation of nxpl::PluginInterface::queryInterface
/*!
    Supports cast to nxcip::CameraDiscoveryManager interface
*/
void* IsdNativePlugin::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraDiscoveryManager, sizeof(nxcip::IID_CameraDiscoveryManager) ) == 0 )
    {
        m_discoveryManager->addRef();
        return m_discoveryManager.get();
    }
    if( memcmp( &interfaceID, &nxpl::IID_Plugin, sizeof( nxpl::IID_Plugin ) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return NULL;
}

unsigned int IsdNativePlugin::addRef()
{
    return m_refManager.addRef();
}

unsigned int IsdNativePlugin::releaseRef()
{
    return m_refManager.releaseRef();
}

const char* IsdNativePlugin::name() const
{
    return isd_edge_settings::paramGroup;
}

void IsdNativePlugin::setSettings( const nxpl::Setting* settings, int count )
{
    const auto settingsEnd = settings + count;
    for( const nxpl::Setting*
        curSetting = settings;
        curSetting < settingsEnd;
        ++curSetting )
    {
        if( strncmp(
                curSetting->name,
                isd_edge_settings::paramGroup,
                sizeof(isd_edge_settings::paramGroup )-1 ) != 0 )
        {
            continue;
        }
        //saving setting
        m_settings.emplace( curSetting->name, curSetting->value );
    }
}

nxpt::CommonRefManager* IsdNativePlugin::refManager()
{
    return &m_refManager;
}

IsdNativePlugin* IsdNativePlugin::instance()
{
    return isdNativePluginInstance;
}
