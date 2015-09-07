/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "include/cdb/connection.h"

#include "connection_factory.h"


extern "C"
{
    nx::cdb::api::ConnectionFactory* createConnectionFactory()
    {
        return new nx::cdb::cl::ConnectionFactory();
    }

    void destroyConnectionFactory(nx::cdb::api::ConnectionFactory* factory)
    {
        delete factory;
    }
}
