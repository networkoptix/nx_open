/**********************************************************
* 15 jul 2014
* akolesnikov
***********************************************************/

#ifndef EC2_THREAD_POOL_H
#define EC2_THREAD_POOL_H

#include <QtCore/QThreadPool>


namespace ec2
{
    class Ec2ThreadPool
    :
        public QThreadPool
    {
    public:
        Ec2ThreadPool();
        ~Ec2ThreadPool();

        static Ec2ThreadPool* instance();
    };
}

#endif  //EC2_THREAD_POOL_H
