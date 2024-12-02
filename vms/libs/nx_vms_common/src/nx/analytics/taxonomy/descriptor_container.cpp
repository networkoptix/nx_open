// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "descriptor_container.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/properties.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

DescriptorContainer::DescriptorContainer(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_propertyWatcher(context->resourcePool())
{
    connect(&m_propertyWatcher, &nx::core::resource::PropertyWatcher::propertyChanged,
        this, &DescriptorContainer::at_descriptorsUpdated);

    m_propertyWatcher.watch(
        {kDescriptorsProperty},
        [](const QnResourcePtr& res)
        {
            return res->hasFlags(Qn::server);
        });
}

/**
 * As we don't have hasTypeEverBeenSupportedInThisScope field in 5.0 and below, now it is always
 * false even if the Object type has ever been supported. To fix this behavior in connection to
 * these old Systems, we update the Scope according to the value hasEverBeenSupported of the Object
 * type.
 */
void DescriptorContainer::fixScopeCompatibility(nx::vms::api::analytics::Descriptors* descriptors)
{
    if (!NX_ASSERT(descriptors))
        return;

    // Do nothing if we are in the Server and its version >= 5.1.0, or if we are in a Client
    // which is connected to a Server with version >= 5.1.0.
    static const nx::utils::SoftwareVersion kEverBeenSupportedInScopeVersion(5, 1);
    const auto connection = systemContext()->messageBusConnection();
    if (!connection || connection->moduleInformation().version >= kEverBeenSupportedInScopeVersion)
        return;

    for (auto& [_, descriptor]: descriptors->objectTypeDescriptors)
    {
        if (!descriptor.hasEverBeenSupported)
            continue;

        for (auto& scope: descriptor.scopes)
            scope.hasTypeEverBeenSupportedInThisScope = true;
    }
}

Descriptors DescriptorContainer::descriptors(const nx::Uuid& serverId)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_cachedDescriptors)
            return *m_cachedDescriptors;
    }

    QnResourcePool* resourcePool = this->resourcePool();
    if (serverId.isNull())
    {
        const auto servers = resourcePool->servers();
        std::map<nx::Uuid, Descriptors> descriptorsByServer;

        Descriptors result;
        for (const QnMediaServerResourcePtr& server: servers)
        {
            const QString serializedDescriptors = server->getProperty(kDescriptorsProperty);
            descriptorsByServer[server->getId()] =
                QJson::deserialized(serializedDescriptors.toUtf8(), Descriptors());
        }

        for (auto& [serverId, descriptors]: descriptorsByServer)
            result.merge(std::move(descriptors));

        fixScopeCompatibility(&result);

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

void DescriptorContainer::updateDescriptors(const Descriptors& descriptors)
{
    QnMediaServerResourcePtr ownServer = resourcePool()->getResourceById<QnMediaServerResource>(
        m_context->peerId());

    if (!NX_ASSERT(ownServer, "Unable to find own mediaserver"))
        return;

    ownServer->setProperty(kDescriptorsProperty, QString::fromUtf8(QJson::serialized(descriptors)));
    ownServer->saveProperties();
}

void DescriptorContainer::updateDescriptorsForTests(Descriptors descriptors)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_cachedDescriptors = std::move(descriptors);
    }

    emit descriptorsUpdated();
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
