// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "include/nx/cloud/db/api/connection.h"

#include <nx/network/cloud/cloud_module_url_fetcher.h>

namespace nx::cloud::db::client {

class ConnectionFactory:
    public api::ConnectionFactory
{
public:
    ConnectionFactory();

    virtual void connect(
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) override;

    virtual std::unique_ptr<api::Connection> createConnection() override;

    virtual std::unique_ptr<api::Connection> createConnection(
        nx::network::http::Credentials credentials) override;

    virtual std::string toString(api::ResultCode resultCode) const override;

    virtual void setCloudUrl(const std::string& url) override;

private:
    network::cloud::CloudDbUrlFetcher m_endPointFetcher;
};

} // nx::cloud::db::client
