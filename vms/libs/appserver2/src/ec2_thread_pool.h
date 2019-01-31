#pragma once

#include <QtCore/QThreadPool>

namespace ec2
{
    // TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2ThreadPool
    class Ec2ThreadPool: public QThreadPool
    {
    public:
        Ec2ThreadPool();

        static Ec2ThreadPool* instance();
    };
}
