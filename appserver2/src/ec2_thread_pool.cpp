/**********************************************************
* 15 jul 2014
* akolesnikov
***********************************************************/

#include "ec2_thread_pool.h"

#include <cassert>


namespace ec2
{
    static Ec2ThreadPool* Ec2ThreadPool_instance = nullptr;

    Ec2ThreadPool::Ec2ThreadPool()
    {
        assert( Ec2ThreadPool_instance == nullptr );
        Ec2ThreadPool_instance = this;
        setMaxThreadCount( QThread::idealThreadCount() );
        setExpiryTimeout(-1); // default expiration timeout is 30 second. But it has a bug in QT < v.5.3
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
