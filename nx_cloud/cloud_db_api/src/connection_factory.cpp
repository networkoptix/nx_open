/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "connection_factory.h"


namespace nx {
namespace cdb {
namespace cl {

void ConnectionFactory::connect(
    const std::string& /*host*/,
    unsigned short /*port*/,
    const std::string& /*login*/,
    const std::string& /*password*/,
    std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler)
{
    //TODO #ak
    completionHandler(
        api::ResultCode::notImplemented,
        std::unique_ptr<api::Connection>());
}

}   //cl
}   //cdb
}   //nx
