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
namespace cl {

ConnectionFactory::ConnectionFactory()
:
    m_endPointFetcher(
        "cdb",
        std::make_unique<nx::network::cloud::RandomOnlineEndpointSelector>())
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
    return std::make_unique<Connection>(&m_endPointFetcher);
}

std::unique_ptr<api::EventConnection> ConnectionFactory::createEventConnection()
{
    return std::make_unique<EventConnection>(&m_endPointFetcher);
}

std::string ConnectionFactory::toString(api::ResultCode resultCode) const
{
    return QnLexical::serialized(resultCode).toStdString();
}

void ConnectionFactory::setCloudEndpoint(
    const std::string& host,
    unsigned short port)
{
    m_endPointFetcher.setEndpoint(SocketAddress(host.c_str(), port));
}

}   //cl
}   //cdb
}   //nx
