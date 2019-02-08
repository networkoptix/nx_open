#include "include/nx/cloud/db/api/connection.h"

#include "connection_factory.h"

extern "C"
{
    nx::cloud::db::api::ConnectionFactory* createConnectionFactory()
    {
        return new nx::cloud::db::client::ConnectionFactory();
    }

    void destroyConnectionFactory(nx::cloud::db::api::ConnectionFactory* factory)
    {
        delete factory;
    }
}
