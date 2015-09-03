/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "include/cdb/connection.h"

#include "connection_factory.h"


extern "C"
{
    nx::cdb::cl::ConnectionFactory* createConnectionFactory()
    {
        return new nx::cdb::cl::ConnectionFactory();
    }

    void destroyConnectionFactory(nx::cdb::cl::ConnectionFactory* factory)
    {
        delete factory;
    }
}
