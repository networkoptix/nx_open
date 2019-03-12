#include "ec2_thread_pool.h"
#include <nx/utils/log/assert.h>

namespace ec2
{
    // In TimeSynchronizationManager::saveSyncTimeAsync a task posted in this thread pool is waiting
    // for another task posted in this pool, which leads to 100% deadlock on a single thread.
    // There is good chance it is not the only such bug. This is an attempt to reduce it's
    // introduction in user environments.
    static const int kMinimalThreadCount = 2;

    Q_GLOBAL_STATIC(Ec2ThreadPool, Ec2ThreadPool_instance)

    Ec2ThreadPool::Ec2ThreadPool()
    {
        setObjectName("ec2::ThreadPool");
        setMaxThreadCount( std::max(QThread::idealThreadCount(), kMinimalThreadCount) );
    }

    Ec2ThreadPool* Ec2ThreadPool::instance()
    {
        return Ec2ThreadPool_instance;
    }
}
