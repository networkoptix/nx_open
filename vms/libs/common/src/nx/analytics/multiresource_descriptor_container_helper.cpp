#include "multiresource_descriptor_container_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace nx::analytics {

MultiresourceDescriptorContainerHelper::MultiresourceDescriptorContainerHelper(
    QnResourcePool* resourcePool,
    OnServerAddedHandler onServerAddedHandler,
    OnServerRemovedHandler onServerRemovedHandler)
    :
    m_onServerAddedHandler(std::move(onServerAddedHandler)),
    m_onServerRemovedHandler(std::move(onServerRemovedHandler))
{
    NX_ASSERT(resourcePool);
    NX_ASSERT(m_onServerAddedHandler);
    NX_ASSERT(m_onServerRemovedHandler);

    connect(
        resourcePool, &QnResourcePool::resourceAdded,
        this, &MultiresourceDescriptorContainerHelper::at_resourceAdded);

    connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        this, &MultiresourceDescriptorContainerHelper::at_resourceRemoved);
}

void MultiresourceDescriptorContainerHelper::at_resourceAdded(QnResourcePtr resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
        m_onServerAddedHandler(server);
}

void MultiresourceDescriptorContainerHelper::at_resourceRemoved(QnResourcePtr resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
        m_onServerRemovedHandler(server);
}


} // namespace nx::analytics
