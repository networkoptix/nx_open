/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#include "add_account_handler.h"

#include <utils/common/cpp14.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "managers/account_manager.h"
#include "managers/types.h"


namespace cdb
{
    const QString AddAccountHttpHandler::HANDLER_PATH = QLatin1String("/add_account");

    AddAccountHttpHandler::~AddAccountHttpHandler()
    {
        int x = 0;
    }

    void AddAccountHttpHandler::processRequest(
        const AuthorizationInfo& authzInfo,
        const data::AccountData& accountData,
        //const stree::AbstractResourceReader& inputParams,
        nx_http::Response* const /*response*/,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            const data::EmailVerificationCode& output )>&& completionHandler )
    {
        //TODO filling data from inputParams?
        AccountManager::instance()->addAccount(
            authzInfo,
            accountData,
            std::bind(
                &AddAccountHttpHandler::addAccountDone,
                this,
                std::placeholders::_1,
                std::move( completionHandler ) ) );
        //TODO #ak
    }

    void AddAccountHttpHandler::addAccountDone(
        ResultCode resultCode,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            const data::EmailVerificationCode& output )> completionHandler )
    {
        //TODO #ak
        completionHandler(
            resultCode == ResultCode::ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::forbidden,
            data::EmailVerificationCode() );
    }
}
