#include "shared_resource_access_provider.h"

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx/utils/log/log.h>
#include <common/common_module.h>

using namespace nx::core::access;

QnSharedResourceAccessProvider::QnSharedResourceAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(mode, parent)
{
    if (mode == Mode::cached)
    {
        connect(sharedResourcesManager(), &QnSharedResourcesManager::sharedResourcesChanged, this,
            &QnSharedResourceAccessProvider::handleSharedResourcesChanged);
    }
}

QnSharedResourceAccessProvider::~QnSharedResourceAccessProvider()
{
}

Source QnSharedResourceAccessProvider::baseSource() const
{
    return Source::shared;
}

bool QnSharedResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->isShared())
        {
            NX_LOG(QnLog::PERMISSIONS_LOG, lit("QnSharedResourceAccessProvider: %1 is not shared, ignore it")
                .arg(layout->getName()),
                cl_logDEBUG1);
            return false;
        }
    }
    else if (!isMediaResource(resource))
    {
        NX_LOG(QnLog::PERMISSIONS_LOG, lit("QnSharedResourceAccessProvider: %1 has invalid type, ignore it")
            .arg(resource->getName()),
            cl_logDEBUG1);
        return false;
    }

    bool result = sharedResourcesManager()->sharedResources(subject).contains(resource->getId());

    NX_LOG(QnLog::PERMISSIONS_LOG, lit("QnSharedResourceAccessProvider: update access %1 to %2: %3")
        .arg(subject.name())
        .arg(resource->getName())
        .arg(result),
        cl_logDEBUG1);

    return result;
}

void QnSharedResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_EXPECT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            &QnSharedResourceAccessProvider::updateAccessToResource);
    }
}

void QnSharedResourceAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_EXPECT(mode() == Mode::cached);

    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto changed = (newValues | oldValues) - (newValues & oldValues);

    QString subjectName = subject.name();

    for (auto resource: commonModule()->resourcePool()->getResources(changed))
    {
        if (newValues.contains(resource->getId()))
        {
            NX_LOG(QnLog::PERMISSIONS_LOG, lit("QnSharedResourceAccessProvider: %1 shared to %2")
                .arg(resource->getName())
                .arg(subjectName),
                cl_logDEBUG1);
        }
        else
        {
            NX_LOG(QnLog::PERMISSIONS_LOG, lit("QnSharedResourceAccessProvider: %1 no more shared to %2")
                .arg(resource->getName())
                .arg(subjectName),
                cl_logDEBUG1);
        }
        updateAccess(subject, resource);
    }
}
