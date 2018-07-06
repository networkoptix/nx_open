/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "connection_factory.h"

#include <nx/utils/std/cpp14.h>
#include <nx/network/cloud/random_online_endpoint_selector.h>

#include "cdb_connection.h"
#include "event_connection.h"

namespace nx {
namespace cdb {
namespace client {

constexpr static auto kDefaultRequestTimeout = std::chrono::seconds(11);

ConnectionFactory::ConnectionFactory()
{
}

void ConnectionFactory::connect(
    std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler)
{
    //TODO #ak downloading xml with urls
    //TODO #ak selecting address to use
    //TODO #ak checking provided credentials at specified address

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
    const std::string& username,
    const std::string& password)
{
    auto connection = createConnection();
    connection->setCredentials(username, password);
    return connection;
}

std::unique_ptr<api::EventConnection> ConnectionFactory::createEventConnection()
{
    return std::make_unique<EventConnection>(&m_endPointFetcher);
}

std::unique_ptr<api::EventConnection> ConnectionFactory::createEventConnection(
    const std::string& username,
    const std::string& password)
{
    auto connection = createEventConnection();
    connection->setCredentials(username, password);
    return connection;
}

std::string ConnectionFactory::toString(api::ResultCode resultCode) const
{
    return QnLexical::serialized(resultCode).toStdString();
}

void ConnectionFactory::setCloudUrl(const std::string& url)
{
    m_endPointFetcher.setUrl(nx::utils::Url(QString::fromStdString(url)));
}

}   //client
}   //cdb
}   //nx
