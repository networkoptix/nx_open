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


namespace cdb
{

class AddAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountData, data::EmailVerificationCode>
{
public:
    static const QString HANDLER_PATH;

protected:
    virtual ~AddAccountHttpHandler();

    virtual void processRequest(
        const AuthorizationInfo& authzInfo,
        const data::AccountData& accountData,
        //const stree::AbstractResourceReader& inputParams,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            const data::EmailVerificationCode& output )>&& completionHandler ) override;

private:
    void addAccountDone(
        ResultCode resultCode,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            const data::EmailVerificationCode& output )> completionHandler );
};

}

#endif  //cloud_db_add_account_handler_h
