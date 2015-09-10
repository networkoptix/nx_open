/**********************************************************
* Sep 3, 2015
* NetworkOptix
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_CONNECTION_H
#define NX_CDB_API_CONNECTION_H

#include <functional>
#include <memory>
#include <string>

#include "account_manager.h"
#include "module_info.h"
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
    virtual void ping(std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler) = 0;
};

//!
class ConnectionFactory
{
public:
    virtual ~ConnectionFactory() {}

    //!Connects to cloud_db to check user credentials
    /*!
        \note Instanciates connection only if connection to cloud is available and provided credentials are valid
    */
    virtual void connect(
        const std::string& login,
        const std::string& password,
        std::function<void(api::ResultCode, std::unique_ptr<api::Connection>)> completionHandler) = 0;
    //!Creates connection object without checking credentials provided
    /*!
        \note No connection to cloud is performed in this method!
    */
    virtual std::unique_ptr<api::Connection> createConnection(
        const std::string& login,
        const std::string& password) = 0;

    //!Explicitely specify endpoint of cloud module. If this method not called, endpoint is detected automatically
    /*!
        \note Call this method only if you are sure about what you are doing
    */
    virtual void setCloudEndpoint(
        const std::string& host,
        unsigned short port) = 0;
};

}   //api
}   //cdb
}   //nx

extern "C"
{
    nx::cdb::api::ConnectionFactory* createConnectionFactory();
    void destroyConnectionFactory(nx::cdb::api::ConnectionFactory* factory);
}

#endif  //NX_CDB_API_CONNECTION_H
