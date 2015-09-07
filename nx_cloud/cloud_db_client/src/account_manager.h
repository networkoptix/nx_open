/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ACCOUNT_MANAGER_H
#define NX_CDB_CL_ACCOUNT_MANAGER_H

#include <deque>

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "data/account_data.h"
#include "include/cdb/account_manager.h"

namespace nx {
namespace cdb {
namespace cl {


class AccountManager
:
    public api::AccountManager,
    public AsyncRequestsExecutor
{
public:
    AccountManager(QUrl url);

    //!Implementation of api::AccountManager::getAccount
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) override;

    void setCredentials(
        const std::string& login,
        const std::string& password);

private:
    mutable QnMutex m_mutex;
    QUrl m_url;
    std::deque<std::unique_ptr<QnStoppableAsync>> m_runningRequests;
        
    QUrl url() const;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_ACCOUNT_MANAGER_H
