/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#include "event_connection.h"

#include <nx/network/cloud/cdb_endpoint_fetcher.h>


namespace nx {
namespace cdb {
namespace cl {

EventConnection::EventConnection(
    network::cloud::CloudModuleEndPointFetcher* const endPointFetcher,
    const std::string& login,
    const std::string& password)
{
}

EventConnection::~EventConnection()
{
}

void EventConnection::start(
    api::SystemEventHandlers eventHandlers,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak connecting with Http client to cloud_db
}

}   //namespace cl
}   //namespace cdb
}   //namespace nx
