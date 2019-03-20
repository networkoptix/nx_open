#include "local_resource_status_watcher.h"

#include <QtCore/QFileInfo>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/file_layout_resource.h>

namespace {

Qn::ResourceStatus getAviResourceStatus(QnAviResourcePtr aviResource)
{
    const QScopedPointer<QnAviArchiveDelegate> archiveDelegate(
        aviResource->createArchiveDelegate());
    return archiveDelegate->open(aviResource) ? Qn::Online : Qn::Offline;
}

Qn::ResourceStatus getFileLayoutResourceStatus(QnFileLayoutResourcePtr layoutResource)
{
    return QFileInfo::exists(layoutResource->getUrl()) ? Qn::Online : Qn::Offline;
}

void setResourceStatus(const QnResourcePtr resource, Qn::ResourceStatus status)
{
    if (resource->getStatus() == status)
        return;

    resource->setStatus(status);
    if (const auto layout = resource.dynamicCast<QnFileLayoutResource>())
    {
        const auto resources = layout->layoutResources();
        for (const auto& item: resources)
        {
            if (item.dynamicCast<QnAviResource>())
                item->setStatus(status);
        }
    }
}

} // namespace

using namespace std::literals::chrono_literals;

QnLocalResourceStatusWatcher::QnLocalResourceStatusWatcher(QObject* parent /*= nullptr*/):
    QObject(parent)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    connect(resourcePool, &QnResourcePool::resourceAdded,
        this, &QnLocalResourceStatusWatcher::onResourceAdded);

    static const auto kUpdatePeriod = 3s;
    m_timerId = startTimer(kUpdatePeriod);
}

QnLocalResourceStatusWatcher::~QnLocalResourceStatusWatcher()
{
}

void QnLocalResourceStatusWatcher::onResourceAdded(const QnResourcePtr& resource)
{
    if (auto aviResource = resource.dynamicCast<QnAviResource>())
    {
        if (aviResource->isEmbedded()) //< Do not watch embedded videos, they are governed by layouts.
            return;

        auto statusEvaluator =
            [aviResource]() { return getAviResourceStatus(aviResource); };

        m_watchedResources.emplace_back(WatchedResource({
            aviResource->getId(),
            std::async(std::launch::async, statusEvaluator),
            statusEvaluator}));
    }
    else if (auto fileLayoutResource = resource.dynamicCast<QnFileLayoutResource>())
    {
        auto statusEvaluator =
            [fileLayoutResource]() { return getFileLayoutResourceStatus(fileLayoutResource); };

        m_watchedResources.emplace_back(WatchedResource({
            fileLayoutResource->getId(),
            std::async(std::launch::async, statusEvaluator),
            statusEvaluator}));
    }
}

void QnLocalResourceStatusWatcher::timerEvent(QTimerEvent* event)
{
    const auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    for (auto it = m_watchedResources.begin(); it != m_watchedResources.end();)
    {
        NX_ASSERT(it->statusFuture.valid());
        if (it->statusFuture.wait_for(std::chrono::milliseconds::zero())
            != std::future_status::ready)
        {
            ++it;
            continue;
        }
        if (auto resource = resourcePool->getResourceById(it->resourceId))
        {
            setResourceStatus(resource, it->statusFuture.get());
            it->statusFuture = std::async(std::launch::async, it->statusEvaluator);
            ++it;
        }
        else
        {
            it = m_watchedResources.erase(it);
        }
    }
}
