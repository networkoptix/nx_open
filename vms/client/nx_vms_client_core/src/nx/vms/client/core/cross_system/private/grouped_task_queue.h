// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPromise>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * Groups tasks in a queue, taking into account the priorities of the groups. Groups are
 * represented by names.
 */
class NX_VMS_CLIENT_CORE_API GroupedTaskQueue: public QObject
{
public:
    /**
     * Promise used to determine whether TaskFunction has finished. It can be used explicitly or
     * implicitly as a guard.
     */
    using Promise = QPromise<void>;

    // Using shared pointers simplifies lambdas significantly.
    using PromisePtr = std::shared_ptr<Promise>;

    /**
     * Task function, which uses Promise to signal about its completion.
     */
    using TaskFunction = std::function<void(PromisePtr)>;

    class NX_VMS_CLIENT_CORE_API Task
    {
    public:
        /**
         * Constructs Task.
         * @param function Function that will be invoked once the queue is ready.
         * @param id Id is used for logging purposes.
         */
        Task(TaskFunction function, const QString& id = {});

        TaskFunction function();
        QString id() const;

    private:
        TaskFunction m_function;
        QString m_id;
    };

    using TaskPtr = std::shared_ptr<Task>;

public:
    explicit GroupedTaskQueue(QObject* parent = nullptr);
    ~GroupedTaskQueue();

    /**
     * Adds a task.
     * @param task Task.
     * @param group Group to which Task belongs.
     */
    void add(TaskPtr task, const QString& group);

    /**
     * Sets the priority of the group.
     * @param group Group.
     * @param priority A higher value indicates higher priority. The default group priority is 0.
     */
    void setPriority(const QString& group, int priority);

    /**
     * @return True if there are tasks in group.
     */
    bool hasTasks(const QString& group) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
