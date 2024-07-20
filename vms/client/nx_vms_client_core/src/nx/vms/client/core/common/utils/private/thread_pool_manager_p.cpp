// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread_pool_manager_p.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

ThreadPool::Manager::Manager(QObject* parent):
    QObject(parent)
{
}

ThreadPool::Manager::~Manager()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_destroyed = true;

    for (const auto pool: m_threadPools)
        delete pool;
}

ThreadPool* ThreadPool::Manager::get(const QString& id)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!NX_ASSERT(!m_destroyed, "Requested a ThreadPool after ThreadPool::Manager is destroyed"))
        return nullptr;

    const auto it = m_threadPools.constFind(id);
    if (it != m_threadPools.cend())
        return *it;

    const auto pool = new ThreadPool(id);
    pool->moveToThread(thread());
    m_threadPools.insert(id, pool);
    return pool;
}

} // namespace nx::vms::client::core
