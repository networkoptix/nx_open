/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_CONNECTION_H
#define NX_CDB_API_CONNECTION_H

#include <functional>
#include <memory>
#include <string>

#include "account_manager.h"
#include "result_code.h"
#include "system_manager.h"


namespace nx {
namespace cdb {
/*!
    Contains classes and methods for manipulating cloud_db data.
    Many methods accept \a completionHandler a an argument. 
    This is functor that is called on operation completion/failure 
    and reports \a ResultCode and output data (if applicable).
    \warning Implementation is allowed to invoke \a completionHandler directly within calling function.
        This allows all this methods to return \a void

    Generally, some methods can be forbidden for credentials.
    E.g., if credentials are system credentials, \a api::AccountManager::getAccount is forbidden
*/
namespace api {

class Connection
{
public:
    virtual ~Connection() {}

    virtual api::AccountManager* accountManager() = 0;
    virtual api::SystemManager* systemManager() = 0;

    //!Set credentials to use
    /*!
        This method does not try to connect to cloud_db check credentials
    */
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) = 0;
    //!Pings cloud_db with current creentials
    virtual void ping(std::function<void(api::ResultCode)> completionHandler) = 0;
};

//!
class ConnectionFactory
{
public:
    virtual ~ConnectionFactory() {}

    //!Connects to cloud_db to check user credentials
    virtual void connect(
        const std::string& login,
        const std::string& password,
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) = 0;
    //!Creates connection object without checking credentials provided
    virtual std::unique_ptr<api::Connection> createConnection(
        const std::string& login,
        const std::string& password) = 0;
};

extern "C"
{
    ConnectionFactory* createConnectionFactory();
    void destroyConnectionFactory(ConnectionFactory* factory);
}

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_CONNECTION_H
