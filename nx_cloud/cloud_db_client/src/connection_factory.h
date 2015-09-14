/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
#define NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H

#include "include/cdb/connection.h"

#include <utils/network/cloud_connectivity/cdb_endpoint_fetcher.h>


namespace nx {
namespace cdb {
//! "cl" stands for "client"
namespace cl {

class ConnectionFactory
:
    public api::ConnectionFactory
{
public:
    ConnectionFactory();

    //!Implementation of \a api::ConnectionFactory::connect
    virtual void connect(
        const std::string& login,
        const std::string& password,
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) override;
    //!Implementation of \a api::ConnectionFactory::createConnection
    virtual std::unique_ptr<api::Connection> createConnection(
        const std::string& login,
        const std::string& password) override;

    //!Implementation of \a api::ConnectionFactory::setCloudEndpoint
    virtual void setCloudEndpoint(
        const std::string& host,
        unsigned short port) override;

private:
    cc::CloudModuleEndPointFetcher m_endPointFetcher;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
