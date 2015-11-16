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
    public nxpl::PluginContainer
{
public:
    CommonPluginContainer();
    virtual ~CommonPluginContainer();

    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    virtual nxpl::TimeProvider* getTimeProvider() override;

private:
    std::atomic<unsigned int> m_refCounter;
};

#endif  //NX_COMMON_PLUGIN_CONTAINER_H
