// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "private/grouped_task_queue.h"

namespace nx::vms::client::core {

/**
 * Scheduler organizes Cloud cross-system requests:
 * - Requests of the first N cloud systems are processed in parallel, one by one for each system.
 * - Requests of the remaining systems are processed one by one in a single queue. Order is
 *     determined by the priority of the system, which can be changed at runtime.
 */
class NX_VMS_CLIENT_CORE_API CloudCrossSystemRequestScheduler: public QObject
{
    Q_OBJECT

public:
    CloudCrossSystemRequestScheduler(QObject* parent = nullptr);
    ~CloudCrossSystemRequestScheduler();

    void add(const QString& cloudSystemId, GroupedTaskQueue::TaskPtr request);
    void add(
        const QString& cloudSystemId,
        GroupedTaskQueue::TaskFunction&& request,
        const QString& requestId);

    void reset();
    void setPriority(const QString& cloudSystemId, int priority);
    bool hasTasks(const QString& cloudSystemId) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
