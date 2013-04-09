/**********************************************************
* 2 apr 2013
* akolesnikov
***********************************************************/

#include <plugins/camera_plugin.h>
#include <plugins/nx_plugin_api.h>

#include "axis_discovery_manager.h"


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
        nxpl::NXPluginInterface* createNXPluginInstance()
    {
        return new AxisCameraDiscoveryManager();
    }
}
