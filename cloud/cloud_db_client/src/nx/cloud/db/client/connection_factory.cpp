// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/random_online_endpoint_selector.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>

#include "cdb_connection.h"

namespace nx::cloud::db::client {

static constexpr auto kDefaultRequestTimeout = std::chrono::seconds(11);

ConnectionFactory::ConnectionFactory():
    m_cloudUrl("https://" + nx::network::SocketGlobals::cloud().cloudHost() + "/")
{
}

ConnectionFactory::~ConnectionFactory() = default;

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
    auto connection = std::make_unique<Connection>(
        m_cloudUrl, nx::network::ssl::kDefaultCertificateCheck);
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
    m_cloudUrl = url;
}

} // nx::cloud::db::client
