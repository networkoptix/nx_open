// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QThreadPool>

namespace nx::vms::client::core {

/**
 * A thread pool managed by a designated manager owned by the application context.
 * The manager ensures that all thread pools live in the main thread and are destroyed with
 * the application context (a safe moment before QCoreApplication destruction).
 *
 * It does not forcefully kill threads, so make sure your threads are short.
 * Use QnLongRunnablePool if your threads do not finish on their own.
 */
class ThreadPool: public QThreadPool
{
    Q_OBJECT
    using base_type = QThreadPool;

    ThreadPool(const QString& id);
    virtual ~ThreadPool() override;

    using base_type::globalInstance; //< Hide QThreadPool::globalInstance method.

public:
    /** Returns thread pool with specified id from the application context. */
    static ThreadPool* instance(const QString& id);

public:
    class Manager;
};

} // namespace nx::vms::client::core
