// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/cloud/db/api/connection.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CloudConnectionFactory
{
public:
    CloudConnectionFactory();

    std::unique_ptr<nx::cloud::db::api::Connection> createConnection() const;

private:
    /* Factory must exist all the time we are using the connection. */
    std::unique_ptr<
        nx::cloud::db::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
};

} // namespace nx::vms::client::core
