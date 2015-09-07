/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_SYSTEM_MANAGER_H
#define NX_CDB_CL_SYSTEM_MANAGER_H

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/cdb/system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

class SystemManager
:
    public api::SystemManager,
    public AsyncRequestsExecutor
{
public:
    SystemManager(QUrl url);

    //!Implementation of \a SystemManager::bindSystem
    virtual void bindSystem(
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;
    //!Implementation of \a SystemManager::unbindSystem
    virtual void unbindSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode)> completionHandler) override;
    //!Implementation of \a SystemManager::getSystems
    virtual void getSystems(
        std::function<void(api::ResultCode, std::vector<api::SystemData>)> completionHandler ) override;
    //!Implementation of \a SystemManager::shareSystem
    virtual void shareSystem(
        api::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler) override;

    void setCredentials(
        const std::string& login,
        const std::string& password);

private:
    mutable QnMutex m_mutex;
    QUrl m_url;

    QUrl getUrl() const;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_SYSTEM_MANAGER_H
