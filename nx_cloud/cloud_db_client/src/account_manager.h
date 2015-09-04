/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ACCOUNT_MANAGER_H
#define NX_CDB_CL_ACCOUNT_MANAGER_H

#include <QtCore/QUrl>

#include "include/cdb/account_manager.h"

#include "data/account_data.h"


namespace nx {
namespace cdb {
namespace cl {


class AccountManager
:
    public api::AccountManager
{
public:
    AccountManager(QUrl url);

    //!Implementation of api::AccountManager::getAccount
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) override;

private:
    QUrl m_url;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_ACCOUNT_MANAGER_H
