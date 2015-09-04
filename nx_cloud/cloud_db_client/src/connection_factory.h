/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
#define NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H

#include "include/cdb/connection.h"


namespace nx {
namespace cdb {
//! "cl" stands for "client"
namespace cl {

class ConnectionFactory
:
    public api::ConnectionFactory
{
public:
    //!Implementation of \a api::ConnectionFactory::connect
    virtual void connect(
        const std::string& login,
        const std::string& password,
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) override;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
