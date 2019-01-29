/**********************************************************
* Nov 16, 2015
* akolesnikov
***********************************************************/

#include <string.h>
#include "common_plugin_container.h"
#include <utils/common/synctime.h>

#include <plugins/plugin_tools.h>

namespace {

class TimeProvider: public nxpt::CommonRefCounter<nxpl::TimeProvider>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override
    {
        if (memcmp(&interfaceID, &nxpl::IID_TimeProvider, sizeof(nxpl::IID_TimeProvider)) == 0)
        {
            addRef();
            return static_cast<nxpl::TimeProvider*>(this);
        }
        if (memcmp(
            &interfaceID,
            &nxpl::IID_PluginInterface,
            sizeof(nxpl::IID_PluginInterface)) == 0)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    virtual uint64_t millisSinceEpoch() const override
    {
        return qnSyncTime->currentMSecsSinceEpoch();
    }
};

} // namespace

CommonPluginContainer::CommonPluginContainer() : m_refCount(1) {}

CommonPluginContainer::~CommonPluginContainer() {}

void* CommonPluginContainer::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxpl::IID_TimeProvider, sizeof(nxpl::IID_TimeProvider)) == 0)
    {
        return new TimeProvider();
    }
    return nullptr;
}

int CommonPluginContainer::addRef() const
{
    return ++m_refCount;
}

int CommonPluginContainer::releaseRef() const
{
    return --m_refCount;
}
