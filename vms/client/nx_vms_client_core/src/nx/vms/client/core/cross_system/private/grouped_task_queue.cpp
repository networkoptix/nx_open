// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grouped_task_queue.h"

#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFuture>
#include <QtCore/QList>
#include <QtCore/QTimer>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {
namespace {

constexpr auto kTimeout = std::chrono::seconds(5);

struct QueueItem
{
    GroupedTaskQueue::TaskPtr task;
    QString group;
    int order = 0;

    bool operator<(const QueueItem& other) const;
};

using GroupPriorities = QHash<QString, int>;

struct Comparator
{
    bool operator()(const QueueItem& left, const QueueItem& right) const
    {
        // Priorities have inverse order.
        const int leftPriority = -priorities[left.group];
        const int rightPriority = -priorities[right.group];

        return std::tie(leftPriority, left.order) < std::tie(rightPriority, right.order);
    }

    GroupPriorities priorities;
};

} // namespace

struct GroupedTaskQueue::Private
{
    GroupedTaskQueue* const q;

    std::set<QueueItem, Comparator> queue;
    GroupPriorities groupPriority;
    QFuture<void> current;
    int taskCounter = 0;
    QElapsedTimer elapsedTimer;
    QTimer timeoutTimer{};
    nx::utils::PendingOperation processNextOperation;

    void add(TaskPtr task, const QString& group);
    void setPriority(const QString& group, int priority);

    void processNext();
    void start(TaskPtr task);
    void scheduleNext();
};

void GroupedTaskQueue::Private::add(TaskPtr task, const QString& group)
{
    if (!NX_ASSERT(task))
        return;

    NX_VERBOSE(this, "Adding task %1 (%2)", task->id(), group);

    queue.insert(QueueItem{.task = task, .group = group, .order = taskCounter++});

    if (!current.isRunning())
    {
        NX_VERBOSE(this, "No current task");
        elapsedTimer.restart();
        scheduleNext();
    }
}

void GroupedTaskQueue::Private::setPriority(const QString& group, int priority)
{
    NX_VERBOSE(this, "Set priority %1 to %2", group, priority);

    groupPriority[group] = priority;
    std::set<QueueItem, Comparator> newQueue(
        queue.begin(), queue.end(), Comparator{.priorities = groupPriority});

    std::swap(queue, newQueue);
}

void GroupedTaskQueue::Private::processNext()
{
    timeoutTimer.stop();

    if (queue.empty())
    {
        NX_VERBOSE(this,
           "Queue is empty, nothing to process. Time elapsed: %1s",
           elapsedTimer.elapsed() / 1000.0);

        current = {};
        return;
    }

    auto task = queue.begin()->task;
    queue.erase(queue.begin());

    start(task);

    timeoutTimer.start();
}

void GroupedTaskQueue::Private::start(TaskPtr task)
{
    NX_VERBOSE(this, "Starting task %1", task->id());
    auto function = task->function();

    if (!NX_ASSERT(function))
        return scheduleNext();

    auto onFinished = nx::utils::guarded(q,
        [this, id = task->id()](QFuture<void> = {})
        {
            NX_VERBOSE(this, "Task %1 finished", id);
            executeInThread(q->thread(), nx::utils::guarded(q, [this]() { scheduleNext(); }));
        });

    auto promise = std::make_shared<Promise>();
    current = promise->future();
    current.then(onFinished).onCanceled(onFinished);
    promise->start();
    function(promise);
}

void GroupedTaskQueue::Private::scheduleNext()
{
    processNextOperation.requestOperation();
}

GroupedTaskQueue::GroupedTaskQueue(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    d->timeoutTimer.setInterval(kTimeout);
    d->timeoutTimer.callOnTimeout(
        [this]()
        {
            NX_VERBOSE(this, "Task timeout");
            d->processNext();
        });

    d->processNextOperation.setInterval(std::chrono::milliseconds(0));
    d->processNextOperation.setFlags(nx::utils::PendingOperation::NoFlags);
    d->processNextOperation.setCallback([this]() { d->processNext(); });
}

GroupedTaskQueue::~GroupedTaskQueue()
{
}

void GroupedTaskQueue::add(TaskPtr task, const QString& group)
{
    d->add(task, group);
}

void GroupedTaskQueue::setPriority(const QString& group, int priority)
{
    d->setPriority(group, priority);
}

bool GroupedTaskQueue::hasTasks(const QString& group) const
{
    return std::any_of(d->queue.begin(), d->queue.end(),
        [group](const QueueItem& item) { return item.group == group; });
}

GroupedTaskQueue::Task::Task(TaskFunction function, const QString& id):
    m_function(std::move(function)),
    m_id(id)
{
}

GroupedTaskQueue::TaskFunction GroupedTaskQueue::Task::function()
{
    return m_function;
}

QString GroupedTaskQueue::Task::id() const
{
    return m_id;
}

} // namespace nx::vms::client::core
