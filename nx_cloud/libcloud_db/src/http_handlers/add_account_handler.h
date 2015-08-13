/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_add_account_handler_h
#define cloud_db_add_account_handler_h

#include <memory>

#include <QString>

#include "access_control/types.h"
#include "base_http_handler.h"
#include "data/account_data.h"
#include "managers/types.h"


namespace nx {
namespace cdb {

class AccountManager;

class AddAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountData>
{
public:
    static const QString HANDLER_PATH;

    AddAccountHttpHandler( AccountManager* const accountManager );
    virtual ~AddAccountHttpHandler();

protected:
    //!Implementation of \a AbstractFiniteMsgBodyHttpHandler::processRequest
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        data::AccountData&& accountData,
        std::function<void( nx_http::StatusCode::Value )>&& completionHandler ) override;

private:
    std::function<void( nx_http::StatusCode::Value )> m_completionHandler;
    AccountManager* const m_accountManager;

    void addAccountDone( ResultCode resultCode );
};

}   //cdb
}   //nx

#endif  //cloud_db_add_account_handler_h
