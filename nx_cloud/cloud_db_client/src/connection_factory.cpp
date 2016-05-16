/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "connection_factory.h"

#include <utils/common/cpp14.h>
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
    const std::string& /*login*/,
    const std::string& /*password*/,
    std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler)
{
    //TODO #ak downloading xml with urls
    //TODO #ak selecting address to use
    //TODO #ak checking provided credentials at specified address

    completionHandler(
        api::ResultCode::notImplemented,
        std::unique_ptr<api::Connection>());
}

std::unique_ptr<api::Connection> ConnectionFactory::createConnection(
    const std::string& login,
    const std::string& password)
{
    return std::make_unique<Connection>(
        &m_endPointFetcher,
        login,
        password);
}

std::unique_ptr<api::EventConnection> ConnectionFactory::createEventConnection(
    const std::string& login,
    const std::string& password)
{
    return std::make_unique<EventConnection>(
        &m_endPointFetcher,
        login,
        password);
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
