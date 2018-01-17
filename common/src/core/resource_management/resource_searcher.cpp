#include "resource_searcher.h"

#include <core/resource/resource_type.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource/resource.h>

QnAbstractResourceSearcher::QnAbstractResourceSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_discoveryMode(DiscoveryMode::fullyEnabled),
    m_localResources(false),
    m_shouldStop(false)
{
}

QnAbstractResourceSearcher::~QnAbstractResourceSearcher()
{
    return;
}

QnResourceList QnAbstractResourceSearcher::search()
{
    m_shouldStop = false;

    auto result = findResources();
    for (const auto& resource: result)
        resource->setCommonModule(commonModule());
    return result;
}

void QnAbstractResourceSearcher::setDiscoveryMode( DiscoveryMode mode )
{
    m_discoveryMode = mode;
}

DiscoveryMode QnAbstractResourceSearcher::discoveryMode() const
{
    const auto& settings = commonModule()->globalSettings();
    if (settings->isInitialized() && settings->isNewSystem())
        return DiscoveryMode::disabled;
    return m_discoveryMode;
}

void QnAbstractResourceSearcher::pleaseStop()
{
    m_shouldStop = true;
}

bool QnAbstractResourceSearcher::shouldStop() const
{
    return m_shouldStop;
}

bool QnAbstractResourceSearcher::isLocal() const
{
    return m_localResources;
}

void QnAbstractResourceSearcher::setLocal(bool l)
{
    m_localResources = l;
}

bool QnAbstractResourceSearcher::isResourceTypeSupported(QnUuid resourceTypeId) const
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
        return false;

    return resourceType->getManufacture() == manufacture();
}


QnAbstractFileResourceSearcher::QnAbstractFileResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule)
{

}

// =============================================================================

QStringList QnAbstractFileResourceSearcher::getPathCheckList() const
{
    QnMutexLocker locker( &m_mutex );
    return m_pathListToCheck;
}

void QnAbstractFileResourceSearcher::setPathCheckList(const QStringList& paths)
{
    QnMutexLocker locker( &m_mutex );
    m_pathListToCheck = paths;
}

void QnAbstractFileResourceSearcher::clearPathCheckList()
{
    setPathCheckList(QStringList());
}

QnResourceList QnAbstractFileResourceSearcher::checkFiles(const QStringList &files) const
{
    QnResourceList result;
    for (const QString &file: files) {
        if (QnResourcePtr res = checkFile(file))
            result.append(res);
    }

    return result;
}

QnAbstractNetworkResourceSearcher::QnAbstractNetworkResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule)
{

}
