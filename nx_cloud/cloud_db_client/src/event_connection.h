/**********************************************************
* Apr 29, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <string>

#include <include/cdb/connection.h>


namespace nx {

namespace network {
namespace cloud {

class CloudModuleEndPointFetcher;

}   //cloud
}   //network

namespace cdb {
namespace cl {

class EventConnection
:
    public api::EventConnection
{
public:
    EventConnection(
        network::cloud::CloudModuleEndPointFetcher* const endPointFetcher,
        const std::string& login,
        const std::string& password);
    virtual ~EventConnection();

    virtual void start(
        api::SystemEventHandlers eventHandlers,
        std::function<void(api::ResultCode)> completionHandler) override;
};

}   //namespace cl
}   //namespace cdb
}   //namespace nx
