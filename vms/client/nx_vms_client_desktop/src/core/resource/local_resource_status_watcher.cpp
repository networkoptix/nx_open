#include "local_resource_status_watcher.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>

Qn::ResourceStatus getAviResourceStatus(QnAviResourcePtr aviResource)
{
    const QScopedPointer<QnAviArchiveDelegate> archiveDelegate(
        aviResource->createArchiveDelegate());
    return archiveDelegate->open(aviResource) ? Qn::Online : Qn::Offline;
}

QnLocalResourceStatusWatcher::QnLocalResourceStatusWatcher(QObject* parent /*= nullptr*/):
    QObject(parent)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    connect(resourcePool, &QnResourcePool::resourceAdded,
        this, &QnLocalResourceStatusWatcher::onResourceAdded);

    static const std::chrono::seconds kUpdatePeriod(1);
    m_timerId = startTimer(kUpdatePeriod);
}

QnLocalResourceStatusWatcher::~QnLocalResourceStatusWatcher()
{
}

void QnLocalResourceStatusWatcher::onResourceAdded(const QnResourcePtr& resource)
{
    if (auto aviResource = resource.template dynamicCast<QnAviResource>())
    {
        m_wathchedResources.emplace_back(std::make_pair(aviResource->getId(),
            std::async(std::launch::async,
            [aviResource]() { return getAviResourceStatus(aviResource); })));
    }
}

void QnLocalResourceStatusWatcher::timerEvent(QTimerEvent* event)
{
    const auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    for (auto it = m_wathchedResources.begin(); it != m_wathchedResources.end();)
    {
        NX_ASSERT(it->second.valid());
        if (it->second.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
        {
            ++it;
            continue;
        }

        if (auto aviResource = resourcePool->getResourceById<QnAviResource>(it->first))
        {
            aviResource->setStatus(it->second.get());
            it->second = std::async(std::launch::async,
                [aviResource]() { return getAviResourceStatus(aviResource); });
            ++it;
        }
        else
        {
            it = m_wathchedResources.erase(it);
        }
    }
}
