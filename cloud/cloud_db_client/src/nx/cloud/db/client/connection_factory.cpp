// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_factory.h"

#include <nx/utils/std/cpp14.h>
#include <nx/network/cloud/random_online_endpoint_selector.h>

#include "cdb_connection.h"

namespace nx::cloud::db::client {

static constexpr auto kDefaultRequestTimeout = std::chrono::seconds(11);

ConnectionFactory::ConnectionFactory() = default;

ConnectionFactory::~ConnectionFactory()
{
    m_endPointFetcher.pleaseStopSync();
}

void ConnectionFactory::connect(
    std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler)
{
    //TODO #akolesnikov downloading xml with urls
    //TODO #akolesnikov selecting address to use
    //TODO #akolesnikov checking provided credentials at specified address

    completionHandler(
        api::ResultCode::notImplemented,
        std::unique_ptr<api::Connection>());
}

std::unique_ptr<api::Connection> ConnectionFactory::createConnection()
{
    std::unique_ptr<api::Connection> connection = std::make_unique<Connection>(&m_endPointFetcher);
    connection->setRequestTimeout(kDefaultRequestTimeout);
    return connection;
}

std::unique_ptr<api::Connection> ConnectionFactory::createConnection(
    nx::network::http::Credentials credentials)
{
    auto connection = createConnection();
    connection->setCredentials(std::move(credentials));
    return connection;
}

std::string ConnectionFactory::toString(api::ResultCode resultCode) const
{
    return nx::reflect::toString(resultCode);
}

void ConnectionFactory::setCloudUrl(const std::string& url)
{
    m_endPointFetcher.setUrl(nx::utils::Url(QString::fromStdString(url)));
}

} // nx::cloud::db::client
