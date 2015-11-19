/**********************************************************
* Nov 16, 2015
* akolesnikov
***********************************************************/

#include <string.h>
#include "common_plugin_container.h"
#include <utils/common/synctime.h>


CommonPluginContainer::CommonPluginContainer() : m_refCount(1) {}

CommonPluginContainer::~CommonPluginContainer() {}

void* CommonPluginContainer::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxpl::IID_TimeProvider, sizeof(nxpl::IID_TimeProvider)) == 0)
    {
        qnSyncTime->addRef();
        return static_cast<nxpl::TimeProvider*>(qnSyncTime);
    }
    return NULL;
}

unsigned int CommonPluginContainer::addRef()
{
    return ++m_refCount;
}

unsigned int CommonPluginContainer::releaseRef() 
{
    return --m_refCount;
}