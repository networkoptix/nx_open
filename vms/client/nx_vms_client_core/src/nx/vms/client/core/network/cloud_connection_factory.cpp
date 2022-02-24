// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_connection_factory.h"

using namespace nx::cloud::db::api;

namespace nx::vms::client::core {

CloudConnectionFactory::CloudConnectionFactory():
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
}

std::unique_ptr<Connection> CloudConnectionFactory::createConnection() const
{
    return connectionFactory->createConnection();
}

} // namespace nx::vms::client::core
