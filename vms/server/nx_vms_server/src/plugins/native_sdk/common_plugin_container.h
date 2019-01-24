/**********************************************************
* Nov 16, 2015
* akolesnikov
***********************************************************/

#ifndef NX_COMMON_PLUGIN_CONTAINER_H
#define NX_COMMON_PLUGIN_CONTAINER_H

#include <atomic>

#include "plugins/plugin_container_api.h"


class CommonPluginContainer
:
    public nxpl::PluginInterface
{
public:
    CommonPluginContainer();
    virtual ~CommonPluginContainer();
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

private:
    mutable std::atomic<int> m_refCount;
};

#endif  //NX_COMMON_PLUGIN_CONTAINER_H
