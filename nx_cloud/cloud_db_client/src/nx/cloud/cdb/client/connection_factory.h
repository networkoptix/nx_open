/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
#define NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H

#include "include/nx/cloud/cdb/api/connection.h"

#include <nx/network/cloud/cloud_module_url_fetcher.h>


namespace nx {
namespace cdb {
namespace client {

class ConnectionFactory:
    public api::ConnectionFactory
{
public:
    ConnectionFactory();

    //!Implementation of \a api::ConnectionFactory::connect
    virtual void connect(
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) override;
    //!Implementation of \a api::ConnectionFactory::createConnection
    virtual std::unique_ptr<api::Connection> createConnection() override;
    virtual std::unique_ptr<api::Connection> createConnection(
        const std::string& username,
        const std::string& password) override;
    virtual std::unique_ptr<api::EventConnection> createEventConnection() override;
    virtual std::unique_ptr<api::EventConnection> createEventConnection(
        const std::string& username,
        const std::string& password) override;
    //!Implementation of \a api::ConnectionFactory::createConnection
    virtual std::string toString(api::ResultCode resultCode) const override;

    //!Implementation of \a api::ConnectionFactory::setCloudUrl
    virtual void setCloudUrl(const std::string& url) override;

private:
    network::cloud::CloudDbUrlFetcher m_endPointFetcher;
};

}   //client
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_CLIENT_CONNECTION_FACTORY_H
