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
