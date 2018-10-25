#include "pointers.h"

namespace nx::mediaserver::sdk_support {

void sdkDeleter(nxpl::PluginInterface* pluginInterface)
{
    if (pluginInterface)
        pluginInterface->releaseRef();
}

} //namespace nx::mediaserver::sdk_support
