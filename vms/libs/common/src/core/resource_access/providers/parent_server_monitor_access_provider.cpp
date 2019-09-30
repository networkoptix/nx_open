#include "parent_server_monitor_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_subject.h>

using namespace nx::core::access;

QnParentServerMonitorAccessProvider::QnParentServerMonitorAccessProvider(
    Mode mode,
    const AccessProviderList& sourceProviders,
    QObject* parent)
    :
    base_type(mode, parent),
    m_sourceProviders(sourceProviders)
{
    for (const auto& sourceProvider: m_sourceProviders)
    {
        connect(sourceProvider, &QnAbstractResourceAccessProvider::accessChanged,
            this, &QnParentServerMonitorAccessProvider::onSourceAccessChanged);
        connect(sourceProvider, &QnAbstractResourceAccessProvider::destroyed, this,
            [this](QObject* object)
            {
                m_sourceProviders.removeAll(
                    static_cast<QnAbstractResourceAccessProvider*>(object));
            });
    }
}

QnParentServerMonitorAccessProvider::~QnParentServerMonitorAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnParentServerMonitorAccessProvider::baseSource() const
{
    return Source::childDevice;
}

bool QnParentServerMonitorAccessProvider::calculateAccess(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    nx::vms::api::GlobalPermissions globalPermissions) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (!resource->flags().testFlag(Qn::server))
        return false;

    const auto childDevices = m_devicesByServerId.value(resource->getId());

    return std::any_of(childDevices.cbegin(), childDevices.cend(),
        [this, subject](const QnResourcePtr& childDevice)
        {
            return hasAccessBySourceProviders(subject, childDevice);
        });
}

void QnParentServerMonitorAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return;

    if (!resource->flags().testFlag(Qn::server))
        return;

    const auto childDevices = m_devicesByServerId.value(resource->getId());

    std::copy_if(childDevices.cbegin(), childDevices.cend(), std::back_inserter(providers),
        [this, subject](const QnResourcePtr& childDevice)
        {
            return hasAccessBySourceProviders(subject, childDevice);
        });
}

void QnParentServerMonitorAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    if (resource.dynamicCast<QnVirtualCameraResource>()
        && !resource->flags().testFlag(Qn::desktop_camera))
    {
        m_devicesByServerId[resource->getParentId()].insert(resource);
        connect(resource, &QnResource::parentIdChanged,
            this, &QnParentServerMonitorAccessProvider::onDeviceParentIdChanged);
    }

    base_type::handleResourceAdded(resource);
}

void QnParentServerMonitorAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (resource.dynamicCast<QnVirtualCameraResource>())
        m_devicesByServerId[resource->getParentId()].remove(resource);

    base_type::handleResourceRemoved(resource);
}

void QnParentServerMonitorAccessProvider::onSourceAccessChanged(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    Source value)
{
    if (resource.dynamicCast<QnVirtualCameraResource>())
    {
        const auto parentResource = resource->getParentResource();
        if (parentResource)
            updateAccess(subject, parentResource);
    }
}

void QnParentServerMonitorAccessProvider::onDeviceParentIdChanged(const QnResourcePtr& resource)
{
    for (auto it = m_devicesByServerId.begin(); it != m_devicesByServerId.end(); ++it)
    {
        if (it.value().remove(resource))
        {
            const auto oldParentServerId = it.key();
            m_devicesByServerId[resource->getParentId()].insert(resource);

            updateAccessToResource(resource->resourcePool()->getResourceById(oldParentServerId));
            updateAccessToResource(resource->getParentResource());

            return;
        }
    }
}

bool QnParentServerMonitorAccessProvider::hasAccessBySourceProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return std::any_of(m_sourceProviders.begin(), m_sourceProviders.end(),
        [subject, resource](const QnAbstractResourceAccessProvider* sourceProvider)
        {
            return sourceProvider->hasAccess(subject, resource);
        });
}
