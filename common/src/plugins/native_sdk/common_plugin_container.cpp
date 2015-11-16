/**********************************************************
* Nov 16, 2015
* akolesnikov
***********************************************************/

#include "common_plugin_container.h"

#include <utils/common/synctime.h>


CommonPluginContainer::CommonPluginContainer()
:
    m_refCounter(1)
{
}

CommonPluginContainer::~CommonPluginContainer()
{
}

unsigned int CommonPluginContainer::addRef()
{
    return ++m_refCounter;
}

unsigned int CommonPluginContainer::releaseRef()
{
    return --m_refCounter;
}

void* CommonPluginContainer::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxpl::IID_PluginContainer, sizeof(nxpl::IID_PluginContainer)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginContainer*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

nxpl::TimeProvider* CommonPluginContainer::getTimeProvider()
{
    return qnSyncTime;
}
