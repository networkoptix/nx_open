// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_request_scheduler.h"

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

struct CloudCrossSystemRequestScheduler::Private
{
    CloudCrossSystemRequestScheduler* const q;
    std::shared_ptr<GroupedTaskQueue> queue;
    QMap<QString /*cloudSystemId*/, std::shared_ptr<GroupedTaskQueue>> parallelQueues;
};

CloudCrossSystemRequestScheduler::CloudCrossSystemRequestScheduler(QObject* parent):
    QObject(parent),
    d(new Private{.q = this, .queue = std::make_shared<GroupedTaskQueue>()})
{
    d->queue->setObjectName("default");
}

CloudCrossSystemRequestScheduler::~CloudCrossSystemRequestScheduler()
{
}

void CloudCrossSystemRequestScheduler::add(
    const QString& cloudSystemId,
    GroupedTaskQueue::TaskPtr request)
{
    if (d->parallelQueues.size() < appContext()->coreSettings()->maxCloudConnections()
        && !d->parallelQueues.contains(cloudSystemId))
    {
        d->parallelQueues[cloudSystemId] = std::make_shared<GroupedTaskQueue>();
    }

    const bool useDefaultQueue = !d->parallelQueues.contains(cloudSystemId);
    GroupedTaskQueue* queue =
        useDefaultQueue ? d->queue.get() : d->parallelQueues[cloudSystemId].get();

    queue->add(request, /*group*/ cloudSystemId);
}

void CloudCrossSystemRequestScheduler::reset()
{
    d->parallelQueues.clear();
    d->queue = std::make_unique<GroupedTaskQueue>();
}

void CloudCrossSystemRequestScheduler::setPriority(const QString& cloudSystemId, int priority)
{
    d->queue->setPriority(cloudSystemId, priority);
}

void CloudCrossSystemRequestScheduler::add(
    const QString& cloudSystemId,
    GroupedTaskQueue::TaskFunction&& request,
    const QString& requestId)
{
    add(/*group*/ cloudSystemId,
        std::make_shared<GroupedTaskQueue::Task>(
        std::move(request), requestId));
}

bool CloudCrossSystemRequestScheduler::hasTasks(const QString& cloudSystemId) const
{
    return d->queue->hasTasks(/*group*/ cloudSystemId) ||
        std::any_of(d->parallelQueues.begin(), d->parallelQueues.end(),
            [cloudSystemId](auto queue) { return queue->hasTasks(/*group*/ cloudSystemId); });
}

} // namespace nx::vms::client::core
