#include "ec2_thread_pool.h"
#include <nx/utils/log/assert.h>

namespace ec2
{
    Q_GLOBAL_STATIC(Ec2ThreadPool, Ec2ThreadPool_instance)

    Ec2ThreadPool::Ec2ThreadPool()
    {
        setMaxThreadCount( QThread::idealThreadCount() );
    }

    Ec2ThreadPool* Ec2ThreadPool::instance()
    {
        return Ec2ThreadPool_instance;
    }
}
