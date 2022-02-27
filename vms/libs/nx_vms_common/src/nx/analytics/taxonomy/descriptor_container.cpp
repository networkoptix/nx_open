// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "descriptor_container.h"

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/analytics/properties.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

DescriptorContainer::DescriptorContainer(QnCommonModule* commonModule):
    Connective<QObject>(commonModule),
    QnCommonModuleAware(commonModule),
    m_propertyWatcher(commonModule->resourcePool())
{
    connect(&m_propertyWatcher, &PropertyWatcher::propertyChanged,
        this, &DescriptorContainer::at_descriptorsUpdated);

    m_propertyWatcher.watch(
        {kDescriptorsProperty},
        [](const QnResourcePtr& res)
        {
            auto server = res.dynamicCast<QnMediaServerResource>();
            return server && server->isCompatible() && !server->flags().testFlag(Qn::fake);
        });
}

Descriptors DescriptorContainer::descriptors(const QnUuid& serverId)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_cachedDescriptors)
            return *m_cachedDescriptors;
    }

    QnResourcePool* resourcePool = commonModule()->resourcePool();
    if (serverId.isNull())
    {
        const auto servers = resourcePool->servers();
        std::map<QnUuid, Descriptors> descriptorsByServer;

        Descriptors result;
        for (const QnMediaServerResourcePtr& server: servers)
        {
            const QString serializedDescriptors = server->getProperty(kDescriptorsProperty);
            descriptorsByServer[server->getId()] =
                QJson::deserialized(serializedDescriptors.toUtf8(), Descriptors());
        }

        for (auto& [serverId, descriptors]: descriptorsByServer)
            result.merge(std::move(descriptors));

        NX_MUTEX_LOCKER lock(&m_mutex);
        m_cachedDescriptors = result;
        return result;
    }

    QnMediaServerResourcePtr server =
        resourcePool->getResourceById<QnMediaServerResource>(serverId);

    if (!NX_ASSERT(server, "Unable to find mediaserver with id %1", serverId))
        return Descriptors();

    const QString serializedDescriptors = server->getProperty(kDescriptorsProperty);
    return QJson::deserialized(serializedDescriptors.toUtf8(), Descriptors());
}

void DescriptorContainer::updateDescriptors(Descriptors descriptors)
{
    QnResourcePool* resourcePool = commonModule()->resourcePool();

    QnMediaServerResourcePtr ownServer = resourcePool->getResourceById<QnMediaServerResource>(
        commonModule()->moduleGUID());

    if (!NX_ASSERT(ownServer, "Unable to find own mediaserver"))
        return;

    ownServer->setProperty(kDescriptorsProperty, QString::fromUtf8(QJson::serialized(descriptors)));
    ownServer->saveProperties();
}

void DescriptorContainer::at_descriptorsUpdated()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_cachedDescriptors.reset();
    }

    emit descriptorsUpdated();
}

} // namespace nx::analytics::taxonomy
