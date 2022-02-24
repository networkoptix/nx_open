// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QThreadPool>

namespace nx::vms::client::core {

/**
 * A thread pool that waits for all threads to finish in QCoreApplication::aboutToQuit.
 * It does not forcefully kill threads, so make sure your threads are short.
 * Use QnLongRunnablePool if your threads do not stop themselves.
 */
class CustomThreadPool: public QThreadPool
{
    Q_OBJECT
    using base_type = QThreadPool;

    CustomThreadPool(const QString& id);
    virtual ~CustomThreadPool() override;

    using base_type::globalInstance; //< Hide QThreadPool::globalInstance method.

public:
    /**
     * Returns thread pool with specified id.
     * Should be called only from the main application thread.
     * Should be called only when QCoreApplication::instance() exists.
     * Can return null if called during application shutdown when all pools are already deleted.
     */
    static CustomThreadPool* instance(const QString& id);

private:
    class Manager;
};

} // namespace nx::vms::client::core
