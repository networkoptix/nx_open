/**********************************************************
* 15 jul 2014
* akolesnikov
***********************************************************/

#include "ec2_thread_pool.h"

#include <nx/utils/log/assert.h>

namespace ec2
{
    static Ec2ThreadPool* Ec2ThreadPool_instance = nullptr;

    Ec2ThreadPool::Ec2ThreadPool()
    {
        NX_ASSERT( Ec2ThreadPool_instance == nullptr );
        Ec2ThreadPool_instance = this;
        setMaxThreadCount( QThread::idealThreadCount() );
    }

    Ec2ThreadPool::~Ec2ThreadPool()
    {
        Ec2ThreadPool_instance = nullptr;
    }

    Ec2ThreadPool* Ec2ThreadPool::instance()
    {
        return Ec2ThreadPool_instance;
    }
}
