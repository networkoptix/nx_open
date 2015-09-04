/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "connection_factory.h"

#include "version.h"


namespace nx {
namespace cdb {
namespace cl {

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

}   //cl
}   //cdb
}   //nx
