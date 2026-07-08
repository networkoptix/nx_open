// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "include/nx/cloud/db/api/connection.h"

#include "connection_factory.h"

extern "C"
{
    nx::cloud::db::api::ConnectionFactory* createConnectionFactory(int idleConnectionsLimit /* = 0 */)
    {
        return new nx::cloud::db::client::ConnectionFactory(idleConnectionsLimit);
    }

    void destroyConnectionFactory(nx::cloud::db::api::ConnectionFactory* factory)
    {
        delete factory;
    }
}
